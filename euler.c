#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

struct body {
    double r[3];
    double v[3];
    double a[3];
    double m;
    int fixed;
};

struct data {
    int nbodies;
    struct body* bodies;
    double G;
    double dt;
};

void euler_next(struct data* data) {
    int n = data->nbodies;
    double G = data->G;
    double dt = data->dt;

    for (int i = 0; i < n; i++) {
        struct body* b1 = &data->bodies[i];
        if (b1->fixed) continue;

        for (int k = 0; k < 3; k++) {
            b1->a[k] = 0;
        }

        for (int j = 0; j < n; j++) {
            if (i == j) continue;

            struct body* b2 = &data->bodies[j];

            double R = 0;
            for (int k = 0; k < 3; k++) {
                R += (b1->r[k] - b2->r[k]) * (b1->r[k] - b2->r[k]);
            }
            R = sqrt(R);

            for (int k = 0; k < 3; k++) {
                b1->a[k] += G * b2->m * (b2->r[k] - b1->r[k]) / R / R / R;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        struct body* b = &data->bodies[i];

        for (int k = 0; k < 3; k++) {
            b->v[k] = b->v[k] + dt * b->a[k];
            b->r[k] = b->r[k] + dt * b->v[k];
        }
    }
}

double kepler(double dt) {
    double G = 1;
    double t;
    double MM = 1e5;

    struct body bodies[] = {
        {
            .r = {0, 0, 0},
            .v = {0, 0, 0},
            .a = {0, 0, 0},
            .m = MM,
            .fixed = 1
        },
        {
            .r = {0, 1, 0},
            .v = {sqrt(G * MM), 0, 0},
            .a = {0, 0, 0},
            .m = 1,
            .fixed = 0
        },
    };

    struct data data = {
        .nbodies = 2,
        .bodies = &bodies[0],
        .G = G,
        .dt = dt
    };

    double max_err = 0;

    for (int i = 0; i < 100; i++) {
        euler_next(&data);
        t = (i + 1) * dt;

        double r = 0;
        for (int k = 0; k < 3; k++) {
            r += data.bodies[1].r[k] * data.bodies[1].r[k];
        }
        r = sqrt(r);
        double err = fabs(r - 1.0);
        if (max_err < err) {
            max_err = err;
        }
    }
    return max_err;
}

void run_test() {
    double err1 = kepler(0.001);
    double err2 = kepler(0.0001);
    double err3 = kepler(0.00001);
    printf("%f %f %f\n", err1, err2, err3);
    if (err1 / 10 < err2) {
        printf("Error1\n");
        exit(1);
    }
    if (err1 / 100 < err3) {
        printf("Error2\n");
        exit(2);
    }
    printf("Ok\n");
    exit(0);
}

void usage(const char* name) {
    printf("%s --input file.txt [--dt 0.001] [--test]\n", name);
    exit(0);
}

int main(int argc, char** argv) {
    const char* fn = NULL;
    double dt = 0.01;
    int test_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (i < argc - 1 && !strcmp(argv[i], "--input")) {
            fn = argv[++i];
        } else if (i < argc - 1 && !strcmp(argv[i], "--dt")) {
            dt = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--test")) {
            test_mode = 1;
        } else {
            usage(argv[0]);
        }
    }
    if (test_mode) {
        run_test(); return 0;
    }
    if (!fn) {
        usage(argv[0]);
    }

    return 0;
}
