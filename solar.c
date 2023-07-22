#include <gtk/gtk.h>
#include <math.h>
#include <limits.h>

double dt = 0.005;
const double G = 2.92e-6;
int Stop = 0;
double Zoom = 10.0;
double ZoomInitial = 10.0;

//typedef float real;
typedef double real;

struct p
{
    real x[3];
};

struct body
{
    real x[3];
    real v[3];
    real a[3];
    real mass;
    int fixed;
    char name[100];
};

struct body body[] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 333333, 1, "Sun"},
    {{0, 0.39, 0}, {1.58, 0, 0}, {0, 0, 0}, 0.038, 0, "Mercury"},
    {{0, 0.72, 0}, {1.17, 0, 0}, {0, 0, 0}, 0.82, 0, "Venus"},
    {{0, 1, 0}, {1, 0, 0}, {0, 0, 0}, 1, 0, "Earth"},
    {{0, 1.00256, 0}, {1.03, 0, 0}, {0, 0, 0}, 0.012, 0, "Moon"},
    {{0, 1.51, 0}, {0.8, 0, 0}, {0, 0, 0}, 0.1, 0, "Mars"},
    {{0, 5.2, 0}, {0.43, 0, 0}, {0, 0, 0}, 317, 0, "Jupiter"},
    {{0, 9.3, 0}, {0.32, 0, 0}, {0, 0, 0}, 95, 0, "Saturn"},
    {{0, 19.3, 0}, {0.23, 0, 0}, {0, 0, 0}, 14.5, 0, "Uranus"},
    {{0, 30, 0}, {0.18, 0, 0}, {0, 0, 0}, 16.7, 0, "Neptune"}};

const int nbodies = 10;

int Max(int a, int b) {
    return a>b?a:b;
}

int Min(int a, int b) {
    return a<b?a:b;
}

void step_verlet() {
    int i, j, k;
    struct p dv_list[nbodies];
    for (i = 0; i < nbodies; ++i)
    {
        struct p new_pos; memset(&new_pos, 0, sizeof(new_pos));
        for (j = 0; j < 3; j++) {
            new_pos.x[j] = body[i].x[j] + dt * body[i].v[j] + 0.5*dt*dt*body[i].a[j];
        }
        dv_list[i] = new_pos;

        struct p new_a; memset(&new_a, 0, sizeof(new_a));
        for (j = 0; j < nbodies; j++) {
            if (i == j) { continue; }
            double R = 0.0;

            struct p *r1 = &new_pos;
            struct body *r2 = &body[j];
            for (k = 0; k < 3; ++k)
            {
                R += (r1->x[k] - r2->x[k]) * (r1->x[k] - r2->x[k]);
            }
            R = sqrt(R);

            for (k = 0; k < 3; ++k)
            {
                new_a.x[k] += G * body[j].mass * (r2->x[k] - r1->x[k]) / R / R / R;
            }
        }

        for (j = 0; j < 3; j++) {
            body[i].v[j] += 0.5 * dt * (new_a.x[j] + body[i].a[j]);
            body[i].a[j] = new_a.x[j];
        }
    }

    for (i = 0; i < nbodies; ++i)
    {
        memcpy(body[i].x, dv_list[i].x, sizeof(dv_list[i].x));
    }
}

void step()
{
    int i, j, k;
    struct p dv_list[nbodies];
    for (i = 0; i < nbodies; ++i)
    {
        struct p dv = {{0, 0, 0}};
        if (body[i].fixed)
        {
            // fixed point
            for (j = 0; j < 3; ++j)
            {
                dv.x[j] = body[i].v[j];
            }
            dv_list[i] = dv;
            continue;
        }

        for (j = 0; j < nbodies; ++j)
        {
            if (i == j)
            {
                continue;
            }

            struct body *r1 = &body[i];
            struct body *r2 = &body[j];
            double R = 0.0;
            for (k = 0; k < 3; ++k)
            {
                R += (r1->x[k] - r2->x[k]) * (r1->x[k] - r2->x[k]);
            }
            R = sqrt(R);

            for (k = 0; k < 3; ++k)
            {
                dv.x[k] += G * body[j].mass * (r2->x[k] - r1->x[k]) / R / R / R;
            }
        }
        dv_list[i] = dv;
    }

    // change velocity
    for (i = 0; i < nbodies; ++i)
    {
        if (body[i].fixed)
        {
            // fixed point
            continue;
        }

        for (k = 0; k < 3; ++k)
        {
            body[i].v[k] += dt * dv_list[i].x[k];
        }
    }

    // change coordinates
    for (i = 0; i < nbodies; ++i)
    {
        if (body[i].fixed)
        {
            // fixed point
            continue;
        }

        for (k = 0; k < 3; ++k)
        {
            body[i].x[k] += dt * body[i].v[k];
        }
    }
}

struct body_ctl
{
    double x0, y0;
    int show_tip;
};

struct App {
    struct body_ctl body_ctls[100];
    GtkWidget* r[3];
    GtkWidget* v[3];
    int active_body;

    GtkWidget* combo;

    GtkWidget* drawing_area;

    guint timer_id;
};

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, struct App* app)
{
    int w = 400, i;
    for (i = 0; i < nbodies; ++i)
    {
        // assume w = h
        double x = body[i].x[0] * w / Zoom + w / 2.0;
        double y = body[i].x[1] * w / Zoom + w / 2.0;
        cairo_arc(cr, x, y, 1, 0, 2 * M_PI);
        cairo_fill(cr);

        app->body_ctls[i].x0 = x;
        app->body_ctls[i].y0 = y;

        if (app->body_ctls[i].show_tip)
        {
            cairo_set_font_size(cr, 13);
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, body[i].name);
        }
    }

    return FALSE;
}

static int get_body(double x, double y, struct App* app) {
    double mindist = -1;
    int argmin = -1;
    int i;
    for (i = 0; i < nbodies; i = i + 1)
    {
        double dist = (app->body_ctls[i].x0 - x) * (app->body_ctls[i].x0 - x) +
                      (app->body_ctls[i].y0 - y) * (app->body_ctls[i].y0 - y);
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

/* Handle button press events by either drawing a rectangle
 * or clearing the surface, depending on which button was pressed.
 * The ::button-press signal handler receives a GdkEventButton
 * struct which contains this information.
 */
static gboolean button_press_event_cb(GtkWidget *widget, GdkEventButton *event, struct App* app)
{
    int argmin = get_body(event->x, event->y, app);
    if (argmin >= 0)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo), argmin);
        app->active_body = argmin;
    }

    /* We've handled the event, stop processing */
    return TRUE;
}

/* Handle motion events by continuing to draw if button 1 is
 * still held down. The ::motion-notify signal handler receives
 * a GdkEventMotion struct which contains this information.
 */
static gboolean motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event, struct App* app)
{
    int i;
    int argmin = get_body(event->x, event->y, app);
    for (i = 0; i < nbodies; i = i + 1)
    {
        app->body_ctls[i].show_tip = 0;
    }
    if (argmin >= 0)
    {
        app->body_ctls[argmin].show_tip = 1;
    }

    /* We've handled it, stop processing */
    return TRUE;
}

static void close_window(GtkWidget* widget, struct App* app)
{
    if (app->timer_id > 0)
    {
        g_source_remove(app->timer_id);
        app->timer_id = 0;
    }

    // gtk_main_quit();
}

gboolean redraw_timeout(struct App *app)
{
    int i, j;

    for (i = 0; i < 5; i = i + 1)
    {
        //step();
        step_verlet();
    }

    char buf[1024];
    i = app->active_body;
    for (j = 0; j < 3; j = j + 1)
    {
        snprintf(buf, sizeof(buf) - 1, "%.16le", body[i].x[j]);
        gtk_entry_set_text(GTK_ENTRY(app->r[j]), buf);
        snprintf(buf, sizeof(buf) - 1, "%.16le", body[i].v[j]);
        gtk_entry_set_text(GTK_ENTRY(app->v[j]), buf);
    }

    int w = 400;
    int minx = INT_MAX, maxx = 0;
    int miny = INT_MAX, maxy = 0;
    for (i = 0; i < nbodies; ++i)
    {
        // assume w = h
        double x = body[i].x[0] * w / Zoom + w / 2.0;
        double y = body[i].x[1] * w / Zoom + w / 2.0;

        minx = Min(minx, x);
        miny = Min(miny, y);
        maxx = Max(maxx, x);
        maxy = Max(maxy, y);
    }

    minx = Max(0, minx-10);
    miny = Max(0, miny-10);
    maxx = maxx + 10;
    maxy = maxy + 10;

    /* Now invalidate the affected region of the drawing area. */
    gtk_widget_queue_draw_area(app->drawing_area, minx, miny, maxx, maxy);

    return app->timer_id > 0;
}

static void
active_changed(GtkComboBox* self, struct App* app)
{
    int active = gtk_combo_box_get_active(self);
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
    Zoom = ZoomInitial * scale;
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));
}

static gboolean mouse_scroll (GtkWidget *widget,
               GdkEvent  *event,
               struct App       *app)
{
    GdkEventScroll * scroll = (GdkEventScroll*) event;

    if (scroll->direction) {
        Zoom = Zoom * 1.1;
    } else {
        Zoom = Zoom * 0.9;
    }
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));

    return TRUE;
}

static void activate(GtkApplication *gapp, gpointer user_data)
{
    struct App* app = user_data;

    int i;
    GtkWidget * window;
    GtkWidget *drawing_area;
    GtkGesture * zoom;

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/solar/exampleapp/theme.css");
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    window = gtk_application_window_new(gapp);
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    drawing_area = gtk_drawing_area_new();

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    //gtk_window_set_child(window, drawing_area);

    gtk_box_pack_start(GTK_BOX(box), drawing_area, TRUE, TRUE, 0);

    GtkWidget* rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), rbox, FALSE, FALSE, 0);

    GtkWidget* combo = gtk_combo_box_text_new();
    for (i=0; i<nbodies; i=i+1) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), body[i].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    g_signal_connect(combo, "changed", G_CALLBACK(active_changed), app);

    gtk_box_pack_start(GTK_BOX(rbox), combo, FALSE, FALSE, 0);
    GtkWidget* x = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rbox), x, FALSE, FALSE, 0);
    GtkWidget* y = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rbox), y, FALSE, FALSE, 0);
    GtkWidget* z = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rbox), z, FALSE, FALSE, 0);
    GtkWidget* vx = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rbox), vx, FALSE, FALSE, 0);
    GtkWidget* vy = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rbox), vy, FALSE, FALSE, 0);
    GtkWidget* vz = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rbox), vz, FALSE, FALSE, 0);
    app->r[0] = x;
    app->r[1] = y;
    app->r[2] = z;
    app->v[0] = vx;
    app->v[1] = vy;
    app->v[2] = vz;
    app->combo = combo;

    g_signal_connect(drawing_area, "draw", G_CALLBACK(draw_cb), app);


    gtk_widget_set_events(
        drawing_area,
        gtk_widget_get_events(drawing_area)
        | GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK | GDK_POINTER_MOTION_MASK);

    // moved to GtkEventControllerMotion
    g_signal_connect(drawing_area, "motion-notify-event", G_CALLBACK(motion_notify_event_cb), app);

    // moved to GtkGestureClick
    g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(button_press_event_cb), app);

    // moved to GtkEventControllerScroll
    g_signal_connect(drawing_area, "scroll-event", G_CALLBACK(mouse_scroll), app);

    zoom = gtk_gesture_zoom_new (drawing_area);

    g_signal_connect(zoom, "begin", G_CALLBACK(zoom_begin_cb), app);
    g_signal_connect(zoom, "scale-changed", G_CALLBACK(zoom_scale_changed_cb), app);

    g_signal_connect(window, "destroy", G_CALLBACK(close_window), app);

    app->drawing_area = drawing_area;
    app->timer_id = g_timeout_add(100, (GSourceFunc)redraw_timeout, app);

    gtk_widget_show_all(window);
}

int main(int argc, char **argv)
{
    struct App app;
    GtkApplication* gapp;
    int status;

    memset(&app, 0, sizeof(app));

    gapp = gtk_application_new ("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (gapp, "activate", G_CALLBACK (activate), &app);
    status = g_application_run (G_APPLICATION (gapp), argc, argv);
    g_object_unref (gapp);

    return status;
}
