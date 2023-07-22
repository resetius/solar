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
    GtkEntryBuffer* r[3];
    GtkEntryBuffer* v[3];
    int active_body;

    GtkWidget* combo;

    GtkWidget* drawing_area;

    guint timer_id;
};

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static void draw_cb(GtkDrawingArea* da, cairo_t *cr, int w1, int h1, void* user_data)
{
    struct App* app = user_data;
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

static void button_press_event_cb(GtkGestureClick* self, int npress, double x, double y, struct App* app)
{
    int argmin = get_body(x, y, app);
    if (argmin >= 0)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo), argmin);
        app->active_body = argmin;
    }
}


static void motion_notify_event_cb(GtkEventControllerMotion* self, double x, double y, struct App* app)
{
    int i;
    int argmin = get_body(x, y, app);
    for (i = 0; i < nbodies; i = i + 1)
    {
        app->body_ctls[i].show_tip = 0;
    }
    if (argmin >= 0)
    {
        app->body_ctls[argmin].show_tip = 1;
    }
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
        gtk_entry_buffer_set_text(app->r[j], buf, strlen(buf));
        snprintf(buf, sizeof(buf) - 1, "%.16le", body[i].v[j]);
        gtk_entry_buffer_set_text(app->v[j], buf, strlen(buf));
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

    gtk_widget_queue_draw(app->drawing_area);

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
    Zoom = ZoomInitial * 1./scale;
    gtk_widget_queue_draw (GTK_WIDGET (app->drawing_area));
}

static gboolean mouse_scroll(
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

    /*
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/solar/exampleapp/theme.css");
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    */

    GtkWidget* window = gtk_application_window_new(gapp);
    //gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(
        GTK_DRAWING_AREA(drawing_area), 800);
    gtk_drawing_area_set_content_height(
        GTK_DRAWING_AREA(drawing_area), 800);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(window, box);

    gtk_box_append(GTK_BOX(box), drawing_area);

    GtkWidget* rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), rbox);

    GtkWidget* combo = gtk_combo_box_text_new();
    for (int i = 0; i<nbodies; i=i+1) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), body[i].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    g_signal_connect(combo, "changed", G_CALLBACK(active_changed), app);

    gtk_box_append(GTK_BOX(rbox), combo);
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
    app->combo = combo;

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

    gtk_widget_set_visible(window, TRUE);
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
