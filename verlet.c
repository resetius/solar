#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

struct body {
    char name[16];
    double r[3];
    double v[3];
    double a[3];
    double a_next[3];
    double m;
    int fixed;
};

struct data {
    int nbodies;
    struct body* bodies;
    double G;
    double dt;
};

void verlet_init(struct data* data) {
    int n = data->nbodies;
    double G = data->G;

    // new acc
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
}

void verlet_next(struct data* data) {
    int n = data->nbodies;
    double G = data->G;
    double dt = data->dt;

    for (int i = 0; i < n; i++) {
        struct body* b = &data->bodies[i];

        for (int k = 0; k < 3; k++) {
            // new pos
            b->r[k] = b->r[k] + b->v[k] * dt + b->a[k] * dt * dt * 0.5;
        }
    }

    // new acc
    for (int i = 0; i < n; i++) {
        struct body* b1 = &data->bodies[i];
        if (b1->fixed) continue;

        for (int k = 0; k < 3; k++) {
            b1->a_next[k] = 0;
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
                b1->a_next[k] += G * b2->m * (b2->r[k] - b1->r[k]) / R / R / R;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        struct body* b = &data->bodies[i];

        for (int k = 0; k < 3; k++) {
            // new vel
            b->v[k] = b->v[k] + 0.5 * dt * (b->a[k] + b->a_next[k]);
            // a = new acc
            b->a[k] = b->a_next[k];
        }
    }
}

double kepler(double dt) {
    double G = 1;
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

    verlet_init(&data);

    for (int i = 0; i < 100; i++) {
        verlet_next(&data);

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
    if (err1 / 97 < err2) {
        printf("Error1 %f\n", err1/err2);
        exit(1);
    }
    if (err1 / 10000 < err3) {
        printf("Error2 %f\n", err1/err3);
        exit(2);
    }
    printf("Ok\n");
    exit(0);
}

/*
  file format:
  G
  N
  Body1 r0 r1 r2 v0 v1 v2 Mass
  Body2 r0 r1 r2 v0 v1 v2 Mass
  ...
  BodyN r0 r1 r2 v0 v1 v2 Mass
 */

void load(struct data* data, const char* fn) {
    FILE* f = fopen(fn, "rb");
    if (!f) { goto err; }

    if (fscanf(f, "%lf %d", &data->G, &data->nbodies) != 2) { goto err; }
    data->bodies = calloc(data->nbodies, sizeof(struct body));
    for (int i = 0; i < data->nbodies; i++) {
        if (fscanf(
                f, "%15s %lf %lf %lf %lf %lf %lf %lf",
                data->bodies[i].name,
                &data->bodies[i].r[0], &data->bodies[i].r[1], &data->bodies[i].r[2],
                &data->bodies[i].v[0], &data->bodies[i].v[1], &data->bodies[i].v[2],
                &data->bodies[i].m) != 8)
        {
            goto err;
        }
    }

    fclose(f);

    return;

err:
    fprintf(stderr, "Cannot open or parse file: '%s'\n", fn);
    exit(1);
}

void print_header(struct data* data) {
    // column names
    printf("t ");
    for (int i = 0; i < data->nbodies; i++) {
        for (int j = 0; j < 3; j++) {
            printf("r%d,%d ", i, j);
        }
        for (int j = 0; j < 3; j++) {
            printf("v%d,%d ", i, j);
        }
    }
    printf("\n");
    // comment
    for (int i = 0; i < data->nbodies; i++) {
        printf("# %s %le\n", data->bodies[i].name, data->bodies[i].m);
    }
}

void print(struct data* data, double t) {
    printf("%e ", t);
    for (int i = 0; i < data->nbodies; i++) {
        printf(
            "%e %e %e %e %e %e ",
            data->bodies[i].r[0], data->bodies[i].r[1], data->bodies[i].r[2],
            data->bodies[i].v[0], data->bodies[i].v[1], data->bodies[i].v[2]);
    }
    printf("\n");
}

void solve(struct data* data, double T) {
    print_header(data);
    print(data, 0);
    int N = T / data->dt;
    verlet_init(data);
    for (int i = 0; i < N; i++) {
        verlet_next(data);
        print(data, (i + 1) * data->dt);
    }
}

void usage(const char* name) {
    printf("%s --input file.txt [--dt 0.001] [--T 10] [--test]\n", name);
    exit(0);
}

int main(int argc, char** argv) {
    const char* fn = NULL;
    double dt = 0.0001;
    double T = 10.0;
    int test_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (i < argc - 1 && !strcmp(argv[i], "--input")) {
            fn = argv[++i];
        } else if (i < argc - 1 && !strcmp(argv[i], "--dt")) {
            dt = atof(argv[++i]);
        } else if (i < argc - 1 && !strcmp(argv[i], "--T")) {
            T = atof(argv[++i]);
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

    struct data data = {.dt = dt};
    load(&data, fn);
    solve(&data, T);
    free(data.bodies);

    return 0;
}
