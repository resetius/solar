#include <gtk/gtk.h>
#include <gio/gio.h>
#include <math.h>
#include <limits.h>
#include <locale.h>

double dt = 0.005;
const double G = 2.92e-6;
int Stop = 0;
double Zoom = 10.0;
double ZoomInitial = 10.0;

//typedef float real;
typedef double real;

int Max(int a, int b) {
    return a>b?a:b;
}

int Min(int a, int b) {
    return a<b?a:b;
}

struct body
{
    double x0, y0; // surface coord
    int show_tip;

    char name[16];
    double r[3];
    double v[3];
    double m;
};

struct App {
    struct body bodies[100];

    GtkEntryBuffer* r[3];
    GtkEntryBuffer* v[3];
    int active_body;

    GtkWidget* drop_down;

    GtkWidget* drawing_area;

    guint timer_id;

    // child
    GSubprocess* subprocess;

    GInputStream* input;
    GDataInputStream* line_input;
    int header_processed;
    int nbodies;
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
        double x = body->r[0] * w / Zoom + w / 2.0;
        double y = body->r[1] * w / Zoom + w / 2.0;
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
            cairo_show_text(cr, body[i].name);
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

static void close_window(GtkWidget* widget, struct App* app)
{
    if (app->timer_id > 0)
    {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
    }
}

gboolean redraw_timeout(struct App *app)
{
    int i, j;

    for (i = 0; i < 5; i = i + 1)
    {
        //step();
        //step_verlet();
    }

    char buf[1024];
    i = app->active_body;
    if (i >= 0 && i < app->nbodies) {
        for (j = 0; j < 3; j = j + 1)
        {
            snprintf(buf, sizeof(buf) - 1, "%.16le", app->bodies[i].r[j]);
            gtk_entry_buffer_set_text(app->r[j], buf, strlen(buf));
            snprintf(buf, sizeof(buf) - 1, "%.16le", app->bodies[i].v[j]);
            gtk_entry_buffer_set_text(app->v[j], buf, strlen(buf));
        }
    }

    int w = 400;
    int minx = INT_MAX, maxx = 0;
    int miny = INT_MAX, maxy = 0;
    for (i = 0; i < app->nbodies; ++i)
    {
        // assume w = h
        double x = app->bodies[i].r[0] * w / Zoom + w / 2.0;
        double y = app->bodies[i].r[1] * w / Zoom + w / 2.0;

        minx = Min(minx, x);
        miny = Min(miny, y);
        maxx = Max(maxx, x);
        maxy = Max(maxy, y);
    }

    minx = Max(0, minx-10);
    miny = Max(0, miny-10);
    maxx = maxx + 10;
    maxy = maxy + 10;

    gtk_widget_queue_draw(app->drawing_area);

    return app->timer_id > 0;
}

static void
active_changed(GtkDropDown* self, GtkStateFlags flags, struct App* app)
{
    int active = gtk_drop_down_get_selected(self);
    app->active_body = active;
}

static void
zoom_begin_cb (GtkGesture       *gesture,
               GdkEventSequence *sequence,
               struct App         *app)
{
    ZoomInitial = Zoom;
}

static void
zoom_scale_changed_cb (GtkGestureZoom *z,
                       gdouble         scale,
                       struct App       *app)
{
    Zoom = ZoomInitial * 1./scale;
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));
}

static void mouse_scroll(
    GtkEventControllerScroll* self,
    double dx, double dy,
    struct App* app)
{
    if (dy > 0) {
        Zoom = Zoom * 1.1;
    } else if (dy < 0) {
        Zoom = Zoom * 0.9;
    }
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));
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

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), box);

    gtk_box_append(GTK_BOX(box), drawing_area);

    GtkWidget* rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), rbox);

    const char* strings[] = { NULL };
    GtkWidget* drop_down = gtk_drop_down_new_from_strings(strings);

    gtk_drop_down_set_selected(GTK_DROP_DOWN(drop_down), 0);
    g_signal_connect(drop_down, "state-flags-changed", G_CALLBACK(active_changed), app);
    gtk_box_append(GTK_BOX(rbox), drop_down);

    for (int i = 0; i < 3; i++) {
        GtkWidget* x = gtk_entry_new();
        gtk_box_append(GTK_BOX(rbox), x);
        app->r[i] = gtk_entry_get_buffer(GTK_ENTRY(x));
    }
    for (int i = 0; i < 3; i++) {
        GtkWidget* vx = gtk_entry_new();
        gtk_box_append(GTK_BOX(rbox), vx);
        app->v[i] = gtk_entry_get_buffer(GTK_ENTRY(vx));
    }
    app->drop_down = drop_down;

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
    app->timer_id = g_timeout_add(100, (GSourceFunc)redraw_timeout, app);

    gtk_window_present(GTK_WINDOW(window));
}

void spawn(struct App* app) {
    const gchar* argv[] = {
        "./euler.exe", "--input", "2bodies.txt", "--dt", "0.001", "--T", "0.1", NULL};

    app->subprocess = g_subprocess_newv(&argv[0], G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL);
    app->input = g_subprocess_get_stdout_pipe(app->subprocess);
    app->line_input = g_data_input_stream_new(app->input);
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
        }

        if (app->header_processed) {
            // parse line
            const char* sep = " ";
            char* p = line;
            printf("Parse: '%s'\n", line);
            p = strtok(p, sep); // skip time
            for (int i = 0; i < app->nbodies; i++) {
                p = strtok(NULL, sep); app->bodies[i].r[0] = atof(p);
                p = strtok(NULL, sep); app->bodies[i].r[1] = atof(p);
                p = strtok(NULL, sep); app->bodies[i].r[2] = atof(p);

                p = strtok(NULL, sep); app->bodies[i].v[0] = atof(p);
                p = strtok(NULL, sep); app->bodies[i].v[1] = atof(p);
                p = strtok(NULL, sep); app->bodies[i].v[2] = atof(p);

                printf("%f %f %f %f %f %f\n",
                       app->bodies[i].r[0],
                       app->bodies[i].r[1],
                       app->bodies[i].r[2],
                       app->bodies[i].v[0],
                       app->bodies[i].v[1],
                       app->bodies[i].v[2]);
            }
        }
        free(line); // performance issue
        read_child(app);
    }
}

void read_child(struct App* app) {
    g_data_input_stream_read_line_async(
        app->line_input,
        /*priority*/ 0, /*cancellable*/ NULL,
        on_new_data, app);
}

int main(int argc, char **argv)
{
    struct App app;
    GtkApplication* gapp;
    int status;

    memset(&app, 0, sizeof(app));

    gtk_disable_setlocale();

    spawn(&app);
    read_child(&app);

    gapp = gtk_application_new ("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (gapp, "activate", G_CALLBACK (activate), &app);

    status = g_application_run (G_APPLICATION (gapp), argc, argv);
    g_object_unref (gapp);

    return status;
}
