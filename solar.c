#include <gtk/gtk.h>
#include <gio/gio.h>
#include <math.h>
#include <limits.h>
#include <locale.h>

struct body
{
    double x0, y0; // surface coord
    int show_tip;

    char name[16];
    double r[3];
    double v[3];
    double m;
};

struct Preset {
    const char* name;
    const char* input_file;
    int method;
    double dt;
};

struct Preset presets[] = {
    {"2 Bodies", "2bodies.txt", 1, 0.00005},
    {"Solar", "solar.txt", 1, 0.005},
    {"Saturn", "saturn.txt", 1, 0.00001}
};

struct App {
    int nbodies;
    struct body bodies[1000];

    GtkLabel* r[3];
    GtkLabel* v[3];
    int active_body;

    GtkWidget* drop_down;

    GtkWidget* drawing_area;

    guint timer_id;

    double zoom;
    double zoom_initial;

    GtkStringList* kernels;

    // kernel settings
    struct Preset* presets;
    int active_preset;

    int method;
    char* input_file;
    double dt;

    // child
    GSubprocess* subprocess;

    GInputStream* input;
    GCancellable* cancel_read;
    GDataInputStream* line_input;
    int header_processed;
    int suspend;
};

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static void draw_cb(GtkDrawingArea* da, cairo_t *cr, int w, int h, void* user_data)
{
    struct App* app = user_data;
    for (int i = 0; i < app->nbodies; ++i)
    {
        // assume w = h
        struct body* body = &app->bodies[i];
        double x = body->r[0] * w * app->zoom + w / 2.0;
        double y = body->r[1] * w * app->zoom + h / 2.0;
        if (app->active_body == i) {
            cairo_set_source_rgb(cr, 1, 0, 0);
        } else {
            cairo_set_source_rgb(cr, 0, 0, 0);
        }
        cairo_arc(cr, x, y, 1, 0, 2 * M_PI);
        cairo_fill(cr);

        body->x0 = x;
        body->y0 = y;

        if (body->show_tip)
        {
            cairo_set_font_size(cr, 13);
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, body->name);
        }
    }
}

static int get_body(double x, double y, struct App* app) {
    double mindist = -1;
    int argmin = -1;
    int i;
    for (i = 0; i < app->nbodies; i = i + 1)
    {
        struct body* body = &app->bodies[i];
        double dist = (body->x0 - x) * (body->x0 - x) +
                      (body->y0 - y) * (body->y0 - y);
        if (argmin < 0 || dist < mindist)
        {
            mindist = dist;
            argmin = i;
        }
    }
    if (sqrt(mindist) < 5)
    {
        return argmin;
    } else {
        return -1;
    }
}

static void button_press_event_cb(GtkGestureClick* self, int npress, double x, double y, struct App* app)
{
    int argmin = get_body(x, y, app);
    if (argmin >= 0)
    {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(app->drop_down), argmin);
        app->active_body = argmin;
    }
}


static void motion_notify_event_cb(GtkEventControllerMotion* self, double x, double y, struct App* app)
{
    int i;
    int argmin = get_body(x, y, app);
    for (i = 0; i < app->nbodies; i = i + 1)
    {
        app->bodies[i].show_tip = 0;
    }
    if (argmin >= 0)
    {
        app->bodies[argmin].show_tip = 1;
    }
}

void stop_kernel(struct App* app) {
    if (app->subprocess) {
        g_cancellable_cancel(app->cancel_read);
        g_subprocess_force_exit(app->subprocess);

        g_input_stream_close(G_INPUT_STREAM(app->line_input), NULL, NULL);
        g_input_stream_close(app->input, NULL, NULL);

        g_object_unref(app->input);
        g_object_unref(app->line_input);
        g_object_unref(app->cancel_read);
        // g_object_unref(app->subprocess);

        app->subprocess = NULL;
    }
}

static void close_window(GtkWidget* widget, struct App* app)
{
    if (app->timer_id > 0)
    {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
    }

    stop_kernel(app);
}


void read_child(struct App* app);

void update_all(struct App* app) {
    char buf[1024];
    int i = app->active_body;
    if (i >= 0 && i < app->nbodies) {
        for (int j = 0; j < 3; j = j + 1)
        {
            snprintf(buf, sizeof(buf) - 1, "<tt>r<sub>%c</sub> = % .8le</tt>", 'x'+j, app->bodies[i].r[j]);
            gtk_label_set_label(app->r[j], buf);
            snprintf(buf, sizeof(buf) - 1, "<tt>v<sub>%c</sub> = % .8le</tt>", 'x'+j, app->bodies[i].v[j]);
            gtk_label_set_label(app->v[j], buf);
        }
    }

    gtk_widget_queue_draw(app->drawing_area);
}

gboolean redraw_timeout(struct App *app)
{
    if (app->header_processed && app->suspend) {
        app->suspend = 0;
        read_child(app);
    }

    return app->timer_id > 0;
}

static void
active_changed(GtkDropDown* self, GtkStateFlags flags, struct App* app)
{
    int active = gtk_drop_down_get_selected(self);
    app->active_body = active;
}

void start_kernel(struct App* app);

void method_changed(GtkDropDown* self, GtkStateFlags flags, struct App* app)
{
    int active = gtk_drop_down_get_selected(self);
    if (active != app->method) {
        app->method = active;
        start_kernel(app);
    }
}

void preset_changed(GtkDropDown* self, GtkStateFlags flags, struct App* app)
{
    int active = gtk_drop_down_get_selected(self);
    if (active != app->active_preset) {
        app->active_preset = active;
    }
}

void input_file_changed(GtkEntry* self, struct App* app) {
    GtkEntryBuffer* buffer = gtk_entry_get_buffer(self);
    const char* text = gtk_entry_buffer_get_text(buffer);
    if (strcmp(text, app->input_file)) {
        free(app->input_file);
        app->input_file = strdup(text);
        start_kernel(app);
    }
}

void dt_changed(GtkSpinButton* self, struct App* app)
{
    double value = gtk_spin_button_get_value(self);
    if (value != app->dt) {
        app->dt = value;
        start_kernel(app);
    }
}

static void
zoom_begin_cb (GtkGesture* gesture,
               GdkEventSequence* sequence,
               struct App* app)
{
    app->zoom_initial = app->zoom;
}

static void
zoom_scale_changed_cb (GtkGestureZoom* z,
                       gdouble scale,
                       struct App* app)
{
    app->zoom = app->zoom_initial * scale;
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));
}

static void mouse_scroll(
    GtkEventControllerScroll* self,
    double dx, double dy,
    struct App* app)
{
    if (dy > 0) {
        app->zoom /= 1.1;
    } else if (dy < 0) {
        app->zoom *= 1.1;
    }
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));
}

GtkWidget* info_widget(struct App* app) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Presets:"));
    const char* presets[] = {"2 Bodies", "Solar", "Saturn", NULL};
    GtkWidget* preset_selector = gtk_drop_down_new_from_strings(presets);
    g_signal_connect(preset_selector, "state-flags-changed", G_CALLBACK(preset_changed), app);
    gtk_box_append(GTK_BOX(box), preset_selector);

    const char* methods[] = {"Euler", "Verlet", NULL};
    gtk_box_append(GTK_BOX(box), gtk_label_new("Method:"));
    GtkWidget* method_selector = gtk_drop_down_new_from_strings(methods);
    g_signal_connect(method_selector, "state-flags-changed", G_CALLBACK(method_changed), app);
    gtk_box_append(GTK_BOX(box), method_selector);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Input:"));
    GtkWidget* entry = gtk_entry_new();
    g_signal_connect(entry, "activate", G_CALLBACK(input_file_changed), app);
    GtkEntryBuffer* buffer = gtk_entry_get_buffer(GTK_ENTRY(entry));
    gtk_entry_buffer_set_text(buffer, app->input_file, strlen(app->input_file));
    gtk_box_append(GTK_BOX(box), entry);

    gtk_box_append(GTK_BOX(box), gtk_label_new("dt:"));
    GtkWidget* dt = gtk_spin_button_new_with_range(1e-14, 0.1, 0.00001);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(dt), 16);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dt), app->dt);
    g_signal_connect(dt, "value_changed", G_CALLBACK(dt_changed), app);
    gtk_box_append(GTK_BOX(box), dt);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Info:"));

    const char* strings[] = { NULL };
    GtkWidget* drop_down = gtk_drop_down_new_from_strings(strings);

    gtk_drop_down_set_selected(GTK_DROP_DOWN(drop_down), 0);
    g_signal_connect(drop_down, "state-flags-changed", G_CALLBACK(active_changed), app);
    gtk_box_append(GTK_BOX(box), drop_down);

    for (int i = 0; i < 3; i++) {
        GtkWidget* x = gtk_label_new("-");
        gtk_box_append(GTK_BOX(box), x);
        app->r[i] = GTK_LABEL(x);
        gtk_label_set_justify(app->r[i], GTK_JUSTIFY_LEFT);
        gtk_label_set_width_chars(app->r[i], 30);
        gtk_label_set_use_markup(app->r[i], TRUE);
    }
    for (int i = 0; i < 3; i++) {
        GtkWidget* vx = gtk_label_new("-");
        gtk_box_append(GTK_BOX(box), vx);
        app->v[i] = GTK_LABEL(vx);
        gtk_label_set_justify(app->v[i], GTK_JUSTIFY_LEFT);
        gtk_label_set_width_chars(app->v[i], 30);
        gtk_label_set_use_markup(app->v[i], TRUE);
    }
    app->drop_down = drop_down;

    gtk_widget_set_halign(box, GTK_ALIGN_END);
    gtk_widget_set_valign(box, GTK_ALIGN_START);

    return box;
}

static void activate(GtkApplication *gapp, gpointer user_data)
{
    struct App* app = user_data;

    GtkWidget* window = gtk_application_window_new(gapp);
    gtk_window_set_title (GTK_WINDOW (window), "Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_widget_set_vexpand(drawing_area, TRUE);
    gtk_widget_set_hexpand(drawing_area, TRUE);

    const char* kernels[] = {
        "./euler.exe --input 2bodies.txt --dt 0.00005 --T 0.1",
        "./verlet.exe --input 2bodies.txt --dt 0.00005 --T 0.1",
        "./euler.exe --input solar.txt --dt 0.005 --T 1e10",
        "./verlet.exe --input solar.txt --dt 0.005 --T 1e10",
        "./verlet.exe --input saturn.txt --dt 0.00001 --T 1e10",
        NULL
    };

    GtkWidget* overlay = gtk_overlay_new();

    gtk_window_set_child(GTK_WINDOW(window), overlay);

    gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), info_widget(app));

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb, app, NULL);

    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(motion_notify_event_cb), app);
    gtk_event_controller_set_propagation_phase(motion, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, motion);

    GtkGesture* gclick = gtk_gesture_click_new();
    g_signal_connect(gclick, "pressed", G_CALLBACK(button_press_event_cb), app);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(gclick), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(gclick));

    GtkEventController* scroll = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(mouse_scroll), app);
    gtk_event_controller_set_propagation_phase(scroll, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, scroll);

    GtkGesture* zoom = gtk_gesture_zoom_new();
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(zoom), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(zoom));

    g_signal_connect(zoom, "begin", G_CALLBACK(zoom_begin_cb), app);
    g_signal_connect(zoom, "scale-changed", G_CALLBACK(zoom_scale_changed_cb), app);

    g_signal_connect(window, "destroy", G_CALLBACK(close_window), app);

    app->drawing_area = drawing_area;
    app->timer_id = g_timeout_add(16, (GSourceFunc)redraw_timeout, app);

    gtk_window_present(GTK_WINDOW(window));
}

void spawn(struct App* app) {
    //gchar** argv = g_strsplit(
    //    gtk_string_list_get_string(app->kernels, app->active_kernel), " ", -1);

    const gchar* exe = app->method == 0
        ? "./euler.exe"
        : "./verlet.exe";
    gchar dt[40];
    snprintf(dt, sizeof(dt) - 1, "%.16e", app->dt);
    const gchar* argv[] = {
        exe,
        "--input", app->input_file,
        "--dt", dt,
        "--T", "1e20",
        NULL};
    printf("run\n");
    int i = 0;
    while (argv[i] != 0) {
        printf("%s\n", argv[i++]);
    }
    app->subprocess = g_subprocess_newv((const gchar**)argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL);
    //g_free(argv);
    app->input = g_subprocess_get_stdout_pipe(app->subprocess);
    app->line_input = g_data_input_stream_new(app->input);
    app->cancel_read = g_cancellable_new();
}

void read_child(struct App* app);

static void on_new_data(GObject* input, GAsyncResult* res, gpointer user_data) {
    struct App* app = user_data;

    gsize size;
    char* line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(input), res, &size, NULL);

    if (line) {
        if (*line == 't') {
            // skip
        } else if (*line == '#') {
            // header
            struct body* body = &app->bodies[app->nbodies++];
            sscanf(line, "# %15s %lf", body->name, &body->m);
        } else if (!app->header_processed) {
            app->header_processed = 1;

            GtkStringList* strings = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(app->drop_down)));
            for (int i = 0; i < app->nbodies; i++) {
                gtk_string_list_append(strings, app->bodies[i].name);
            }
            app->active_body = 0;
        }

        if (app->header_processed) {
            // parse line
            const char* sep = " ";
            char* p = line;
            p = strtok(p, sep); // skip time
            for (int i = 0; p && i < app->nbodies; i++) {
                if ((p = strtok(NULL, sep))) app->bodies[i].r[0] = atof(p);
                if ((p = strtok(NULL, sep))) app->bodies[i].r[1] = atof(p);
                if ((p = strtok(NULL, sep))) app->bodies[i].r[2] = atof(p);

                if ((p = strtok(NULL, sep))) app->bodies[i].v[0] = atof(p);
                if ((p = strtok(NULL, sep))) app->bodies[i].v[1] = atof(p);
                if ((p = strtok(NULL, sep))) app->bodies[i].v[2] = atof(p);
            }

            update_all(app);
            app->suspend = 1;
        }
        free(line); // performance issue

        if (!app->suspend) {
            read_child(app);
        }
    }
}

void read_child(struct App* app) {
    g_data_input_stream_read_line_async(
        app->line_input,
        /*priority*/ 0, app->cancel_read,
        on_new_data, app);
}

void start_kernel(struct App* app) {
    stop_kernel(app);

    GtkStringList* strings = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(app->drop_down)));
    gtk_string_list_splice(strings, 0, app->nbodies, NULL);
    app->nbodies = 0;
    app->active_body = -1;
    app->header_processed = 0;
    app->suspend = 0;

    spawn(app);
    read_child(app);
}

int main(int argc, char **argv)
{
    struct App app;
    GtkApplication* gapp;
    int status;

    memset(&app, 0, sizeof(app));
    app.zoom = app.zoom_initial = 0.1;

    app.method = -1;
    app.input_file = strdup("2bodies.txt");
    app.dt = 1e-5;

    gtk_disable_setlocale();

    gapp = gtk_application_new ("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (gapp, "activate", G_CALLBACK (activate), &app);

    status = g_application_run (G_APPLICATION (gapp), argc, argv);
    g_object_unref (gapp);

    return status;
}
