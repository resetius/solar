#include <gtk/gtk.h>
#include <gio/gio.h>
#include <math.h>
#include <limits.h>
#include <locale.h>

struct body
{
    double x0;
    double y0;
    int show_tip;

    char name[16];
    double r[3];
    double v[3];
    double m;
};

struct preset {
    const char* name;
    const char* input_file;
    int method;
    double dt;
};

struct context {
    int nbodies;
    struct body bodies[1000];

    GtkLabel* r[3];
    GtkLabel* v[3];
    int active_body;

    GtkWidget* body_selector;
    GtkWidget* drawing_area;

    guint timer_id;

    double zoom;
    double zoom_initial;

    GtkStringList* kernels;

    // kernel settings
    struct preset* presets;
    int active_preset;

    int method;
    char input_file[100];
    double dt;

    // controls
    GtkWidget* method_selector;
    GtkEntryBuffer* input_file_entry;
    GtkWidget* dt_selector;

    // child
    GSubprocess* subprocess;

    GInputStream* input;
    GCancellable* cancel_read;
    GDataInputStream* line_input;
    int header_processed;
    int suspend;
};

void draw(GtkDrawingArea* da, cairo_t *cr, int w, int h, void* user_data)
{
    struct context* ctx = user_data;
    for (int i = 0; i < ctx->nbodies; ++i)
    {
        // assume w = h
        struct body* body = &ctx->bodies[i];
        double x = body->r[0] * w * ctx->zoom + w / 2.0;
        double y = body->r[1] * w * ctx->zoom + h / 2.0;
        if (ctx->active_body == i) {
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

int get_body(double x, double y, struct context* ctx) {
    double mindist = -1;
    int argmin = -1;
    int i;
    for (i = 0; i < ctx->nbodies; i = i + 1)
    {
        struct body* body = &ctx->bodies[i];
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

void button_press(GtkGestureClick* self, int npress, double x, double y, struct context* ctx)
{
    int index = get_body(x, y, ctx);
    if (index >= 0)
    {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->body_selector), index);
        ctx->active_body = index;
    }
}


void motion_notify(GtkEventControllerMotion* self, double x, double y, struct context* ctx)
{
    int i;
    int argmin = get_body(x, y, ctx);
    for (i = 0; i < ctx->nbodies; i = i + 1)
    {
        ctx->bodies[i].show_tip = 0;
    }
    if (argmin >= 0)
    {
        ctx->bodies[argmin].show_tip = 1;
    }
}

void stop_kernel(struct context* ctx) {
    if (ctx->subprocess) {
        g_cancellable_cancel(ctx->cancel_read);
        g_subprocess_force_exit(ctx->subprocess);

        g_input_stream_close(G_INPUT_STREAM(ctx->line_input), NULL, NULL);
        g_input_stream_close(ctx->input, NULL, NULL);

        g_object_unref(ctx->input);
        g_object_unref(ctx->line_input);
        g_object_unref(ctx->cancel_read);
        // g_object_unref(ctx->subprocess);

        ctx->subprocess = NULL;
    }
}

void close_window(GtkWidget* widget, struct context* ctx)
{
    if (ctx->timer_id > 0)
    {
        g_source_remove(ctx->timer_id);
        ctx->timer_id = 0;
    }

    stop_kernel(ctx);
}

void read_child(struct context* ctx);

void update_all(struct context* ctx) {
    char buf[1024];
    int i = ctx->active_body;
    if (i >= 0 && i < ctx->nbodies) {
        for (int j = 0; j < 3; j = j + 1)
        {
            snprintf(buf, sizeof(buf) - 1, "<tt>r<sub>%c</sub> = % .8le</tt>", 'x'+j, ctx->bodies[i].r[j]);
            gtk_label_set_label(ctx->r[j], buf);
            snprintf(buf, sizeof(buf) - 1, "<tt>v<sub>%c</sub> = % .8le</tt>", 'x'+j, ctx->bodies[i].v[j]);
            gtk_label_set_label(ctx->v[j], buf);
        }
    }

    gtk_widget_queue_draw(ctx->drawing_area);
}

void on_new_data(GObject* input, GAsyncResult* res, gpointer user_data) {
    struct context* ctx = user_data;

    gsize size;
    char* line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(input), res, &size, NULL);

    if (line) {
        if (*line == 't') {
            // skip column names
        } else if (*line == '#' && ctx->nbodies < sizeof(ctx->bodies)/sizeof(struct body)) {
            // header
            struct body* body = &ctx->bodies[ctx->nbodies++];
            sscanf(line, "# %15s %lf", body->name, &body->m);
        } else if (!ctx->header_processed) {
            ctx->header_processed = 1;

            GtkStringList* strings = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(ctx->body_selector)));
            for (int i = 0; i < ctx->nbodies; i++) {
                gtk_string_list_append(strings, ctx->bodies[i].name);
            }
            ctx->active_body = 0;
        }

        if (ctx->header_processed) {
            // parse line
            const char* sep = " ";
            char* p = line;
            p = strtok(p, sep); // skip time
            for (int i = 0; p && i < ctx->nbodies; i++) {
                if ((p = strtok(NULL, sep))) ctx->bodies[i].r[0] = atof(p);
                if ((p = strtok(NULL, sep))) ctx->bodies[i].r[1] = atof(p);
                if ((p = strtok(NULL, sep))) ctx->bodies[i].r[2] = atof(p);

                if ((p = strtok(NULL, sep))) ctx->bodies[i].v[0] = atof(p);
                if ((p = strtok(NULL, sep))) ctx->bodies[i].v[1] = atof(p);
                if ((p = strtok(NULL, sep))) ctx->bodies[i].v[2] = atof(p);
            }

            update_all(ctx);
            ctx->suspend = 1;
        }
        g_free(line); // performance issue

        if (!ctx->suspend) {
            read_child(ctx);
        }
    }
}

void read_child(struct context* ctx) {
    g_data_input_stream_read_line_async(
        ctx->line_input,
        /*priority*/ 0, ctx->cancel_read,
        on_new_data, ctx);
}

gboolean timeout(struct context* ctx)
{
    if (ctx->header_processed && ctx->suspend) {
        ctx->suspend = 0;
        read_child(ctx);
    }

    return ctx->timer_id > 0;
}

void active_changed(GtkDropDown* self, GtkStateFlags flags, struct context* ctx)
{
    int active = gtk_drop_down_get_selected(self);
    ctx->active_body = active;
}

void spawn(struct context* ctx) {
    const gchar* exe = ctx->method == 0
        ? "./euler.exe"
        : "./verlet.exe";
    gchar dt[40];
    snprintf(dt, sizeof(dt) - 1, "%.16e", ctx->dt);
    const gchar* argv[] = {
        exe,
        "--input", ctx->input_file,
        "--dt", dt,
        "--T", "1e20",
        NULL};
    ctx->subprocess = g_subprocess_newv((const gchar**)argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL);
    ctx->input = g_subprocess_get_stdout_pipe(ctx->subprocess);
    ctx->line_input = g_data_input_stream_new(ctx->input);
    ctx->cancel_read = g_cancellable_new();
}

void start_kernel(struct context* ctx) {
    stop_kernel(ctx);

    GtkStringList* strings = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(ctx->body_selector)));
    gtk_string_list_splice(strings, 0, ctx->nbodies, NULL);
    ctx->nbodies = 0;
    ctx->active_body = -1;
    ctx->header_processed = 0;
    ctx->suspend = 0;

    spawn(ctx);
    read_child(ctx);
}

void method_changed(GtkDropDown* self, GtkStateFlags flags, struct context* ctx)
{
    int active = gtk_drop_down_get_selected(self);
    if (active != ctx->method) {
        ctx->method = active;
        start_kernel(ctx);
    }
}

void preset_changed(GtkDropDown* self, GtkStateFlags flags, struct context* ctx)
{
    int active = gtk_drop_down_get_selected(self);
    if (active != ctx->active_preset) {
        ctx->active_preset = active;
        struct preset* preset = &ctx->presets[active];
        ctx->method = preset->method;
        ctx->dt = preset->dt;
        strncpy(ctx->input_file, preset->input_file, sizeof(ctx->input_file));
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->method_selector), preset->method);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->dt_selector), preset->dt);
        gtk_entry_buffer_set_text(ctx->input_file_entry, preset->input_file, strlen(preset->input_file));
        start_kernel(ctx);
    }
}

void input_file_changed(GtkEntry* self, struct context* ctx) {
    GtkEntryBuffer* buffer = gtk_entry_get_buffer(self);
    const char* text = gtk_entry_buffer_get_text(buffer);
    if (strcmp(text, ctx->input_file)) {
        strncpy(ctx->input_file, text, sizeof(ctx->input_file));
        start_kernel(ctx);
    }
}

void dt_changed(GtkSpinButton* self, struct context* ctx)
{
    double value = gtk_spin_button_get_value(self);
    if (value != ctx->dt) {
        ctx->dt = value;
        start_kernel(ctx);
    }
}

void zoom_begin(GtkGesture* gesture, GdkEventSequence* sequence, struct context* ctx)
{
    ctx->zoom_initial = ctx->zoom;
}

void zoom_scale_changed(GtkGestureZoom* z, gdouble scale, struct context* ctx)
{
    ctx->zoom = ctx->zoom_initial * scale;
    gtk_widget_queue_draw (GTK_WIDGET (ctx->drawing_area));
}

void mouse_scroll(GtkEventControllerScroll* self, double dx, double dy, struct context* ctx)
{
    if (dy > 0) {
        ctx->zoom /= 1.1;
    } else if (dy < 0) {
        ctx->zoom *= 1.1;
    }
    gtk_widget_queue_draw (GTK_WIDGET (ctx->drawing_area));
}

GtkWidget* control_widget(struct context* ctx) {
    GtkWidget* frame = gtk_frame_new("Controls");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_frame_set_child(GTK_FRAME(frame), box);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Preset:"));
    const char* presets[] = {"2 Bodies", "Solar", "Saturn", NULL};
    GtkWidget* preset_selector = gtk_drop_down_new_from_strings(presets);
    g_signal_connect(preset_selector, "state-flags-changed", G_CALLBACK(preset_changed), ctx);
    gtk_box_append(GTK_BOX(box), preset_selector);

    const char* methods[] = {"Euler", "Verlet", NULL};
    gtk_box_append(GTK_BOX(box), gtk_label_new("Method:"));
    GtkWidget* method_selector = ctx->method_selector = gtk_drop_down_new_from_strings(methods);
    g_signal_connect(method_selector, "state-flags-changed", G_CALLBACK(method_changed), ctx);
    gtk_box_append(GTK_BOX(box), method_selector);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Input:"));
    GtkWidget* entry = gtk_entry_new();
    g_signal_connect(entry, "activate", G_CALLBACK(input_file_changed), ctx);
    GtkEntryBuffer* buffer = ctx->input_file_entry = gtk_entry_get_buffer(GTK_ENTRY(entry));
    gtk_entry_buffer_set_text(buffer, ctx->input_file, strlen(ctx->input_file));
    gtk_box_append(GTK_BOX(box), entry);

    gtk_box_append(GTK_BOX(box), gtk_label_new("dt:"));
    GtkWidget* dt = ctx->dt_selector = gtk_spin_button_new_with_range(1e-14, 0.1, 0.00001);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(dt), 8);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dt), ctx->dt);
    g_signal_connect(dt, "value_changed", G_CALLBACK(dt_changed), ctx);
    gtk_box_append(GTK_BOX(box), dt);

    return frame;
}

GtkWidget* info_widget(struct context* ctx) {
    GtkWidget* frame = gtk_frame_new("Info");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_frame_set_child(GTK_FRAME(frame), box);

    const char* strings[] = { NULL };
    GtkWidget* body_selector = ctx->body_selector = gtk_drop_down_new_from_strings(strings);

    gtk_drop_down_set_selected(GTK_DROP_DOWN(body_selector), 0);
    g_signal_connect(body_selector, "state-flags-changed", G_CALLBACK(active_changed), ctx);
    gtk_box_append(GTK_BOX(box), body_selector);

    for (int i = 0; i < 3; i++) {
        GtkWidget* x = gtk_label_new("-");
        gtk_box_append(GTK_BOX(box), x);
        ctx->r[i] = GTK_LABEL(x);
        gtk_label_set_width_chars(ctx->r[i], 30);
        gtk_label_set_use_markup(ctx->r[i], TRUE);
    }
    for (int i = 0; i < 3; i++) {
        GtkWidget* vx = gtk_label_new("-");
        gtk_box_append(GTK_BOX(box), vx);
        ctx->v[i] = GTK_LABEL(vx);
        gtk_label_set_width_chars(ctx->v[i], 30);
        gtk_label_set_use_markup(ctx->v[i], TRUE);
    }

    return frame;
}

GtkWidget* right_pane(struct context* ctx) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_append(GTK_BOX(box), control_widget(ctx));
    gtk_box_append(GTK_BOX(box), info_widget(ctx));

    gtk_widget_set_halign(box, GTK_ALIGN_END);
    gtk_widget_set_valign(box, GTK_ALIGN_START);

    return box;
}

void activate(GtkApplication* app, gpointer user_data)
{
    struct context* ctx = user_data;

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css_provider, "style.css");

    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css_provider), 0);

    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title (GTK_WINDOW (window), "N-Body");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_widget_set_vexpand(drawing_area, TRUE);
    gtk_widget_set_hexpand(drawing_area, TRUE);

    GtkWidget* overlay = gtk_overlay_new();

    gtk_window_set_child(GTK_WINDOW(window), overlay);

    gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), right_pane(ctx));

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw, ctx, NULL);

    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(motion_notify), ctx);
    gtk_event_controller_set_propagation_phase(motion, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, motion);

    GtkGesture* gclick = gtk_gesture_click_new();
    g_signal_connect(gclick, "pressed", G_CALLBACK(button_press), ctx);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(gclick), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(gclick));

    GtkEventController* scroll = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(mouse_scroll), ctx);
    gtk_event_controller_set_propagation_phase(scroll, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, scroll);

    GtkGesture* zoom = gtk_gesture_zoom_new();
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(zoom), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(zoom));

    g_signal_connect(zoom, "begin", G_CALLBACK(zoom_begin), ctx);
    g_signal_connect(zoom, "scale-changed", G_CALLBACK(zoom_scale_changed), ctx);

    g_signal_connect(window, "destroy", G_CALLBACK(close_window), ctx);

    ctx->drawing_area = drawing_area;
    ctx->timer_id = g_timeout_add(16, (GSourceFunc)timeout, ctx);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    struct preset presets[] = {
        {"2 Bodies", "2bodies.txt", 1, 0.00005},
        {"Solar", "solar.txt", 1, 0.005},
        {"Saturn", "saturn.txt", 1, 0.00001}
    };

    struct context ctx;
    GtkApplication* app;
    int status;

    memset(&ctx, 0, sizeof(ctx));
    ctx.zoom = ctx.zoom_initial = 0.1;

    ctx.active_preset = -1;
    ctx.method = -1;
    strncpy(ctx.input_file, "2bodies.txt", sizeof(ctx.input_file));
    ctx.dt = 1e-5;
    ctx.presets = presets;

    gtk_disable_setlocale();

    app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref (app);

    return status;
}
