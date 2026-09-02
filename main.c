#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <pthread.h>

#define REAL_MIN -2.0
#define REAL_MAX  1.0
#define IMAG_MIN -1.5
#define IMAG_MAX  1.5

typedef struct {
    int largura;
    int altura;
    int max_iter;
    int num_threads;
} Config;

typedef struct {
    int id;
    Config cfg;
    unsigned char *img;
} ThreadData;

static inline unsigned char mandelbrot_pixel(int x, int y, Config cfg) {
    double cr = REAL_MIN + (double)x * (REAL_MAX - REAL_MIN) / cfg.largura;
    double ci = IMAG_MIN + (double)y * (IMAG_MAX - IMAG_MIN) / cfg.altura;
    
    double zr = 0.0, zi = 0.0;
    int iter = 0;

    while ((zr * zr + zi * zi <= 4.0) && (iter < cfg.max_iter)) {
        double tmp = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tmp;
        iter++;
    }

    return (unsigned char)(((double)iter / cfg.max_iter) * 255.0);
}

static int salvar_pgm(const char *path, const unsigned char *img, int w, int h) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Erro ao criar arquivo de saida: %s\n", path);
        return 0;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            fprintf(f, "%u%s", img[y * w + x], (x == w - 1) ? "" : " ");
        }
        fputc('\n', f);
    }

    fclose(f);
    return 1;
}

static void run_serial(Config cfg, unsigned char *img) {
    for (int y = 0; y < cfg.altura; y++) {
        for (int x = 0; x < cfg.largura; x++) {
            img[y * cfg.largura + x] = mandelbrot_pixel(x, y, cfg);
        }
    }
}

static void run_openmp(Config cfg, unsigned char *img) {
    #pragma omp parallel for schedule(static) num_threads(cfg.num_threads)
    for (int y = 0; y < cfg.altura; y++) {
        for (int x = 0; x < cfg.largura; x++) {
            img[y * cfg.largura + x] = mandelbrot_pixel(x, y, cfg);
        }
    }
}

static void *worker_blocks(void *arg) {
    ThreadData *td = (ThreadData *)arg;
    int h = td->cfg.altura;
    int n = td->cfg.num_threads;
    
    int chunk = h / n;
    int start_y = td->id * chunk;
    int end_y = (td->id == n - 1) ? h : start_y + chunk;

    for (int y = start_y; y < end_y; y++) {
        for (int x = 0; x < td->cfg.largura; x++) {
            td->img[y * td->cfg.largura + x] = mandelbrot_pixel(x, y, td->cfg);
        }
    }
    return NULL;
}

static int run_pthreads_blocks(Config cfg, unsigned char *img) {
    pthread_t *threads = malloc(cfg.num_threads * sizeof(pthread_t));
    ThreadData *tdata = malloc(cfg.num_threads * sizeof(ThreadData));
    
    if (!threads || !tdata) {
        fprintf(stderr, "Erro de alocação de memória para threads.\n");
        free(threads);
        free(tdata);
        return 0;
    }

    for (int i = 0; i < cfg.num_threads; i++) {
        tdata[i].id = i;
        tdata[i].cfg = cfg;
        tdata[i].img = img;
        if (pthread_create(&threads[i], NULL, worker_blocks, &tdata[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread %d.\n", i);
            free(threads);
            free(tdata);
            return 0;
        }
    }

    for (int i = 0; i < cfg.num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(tdata);
    return 1;
}

static void *worker_cyclic(void *arg) {
    ThreadData *td = (ThreadData *)arg;

    for (int y = td->id; y < td->cfg.altura; y += td->cfg.num_threads) {
        for (int x = 0; x < td->cfg.largura; x++) {
            td->img[y * td->cfg.largura + x] = mandelbrot_pixel(x, y, td->cfg);
        }
    }
    return NULL;
}

static int run_pthreads_cyclic(Config cfg, unsigned char *img) {
    pthread_t *threads = malloc(cfg.num_threads * sizeof(pthread_t));
    ThreadData *tdata = malloc(cfg.num_threads * sizeof(ThreadData));

    if (!threads || !tdata) {
        fprintf(stderr, "Erro de alocação de memória para threads.\n");
        free(threads);
        free(tdata);
        return 0;
    }

    for (int i = 0; i < cfg.num_threads; i++) {
        tdata[i].id = i;
        tdata[i].cfg = cfg;
        tdata[i].img = img;
        if (pthread_create(&threads[i], NULL, worker_cyclic, &tdata[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread %d.\n", i);
            free(threads);
            free(tdata);
            return 0;
        }
    }

    for (int i = 0; i < cfg.num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(tdata);
    return 1;
}

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec / 1e9);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <largura> <altura> <max_iteracoes> <num_threads>\n", argv[0]);
        return 1;
    }

    Config cfg;
    cfg.largura = atoi(argv[1]);
    cfg.altura = atoi(argv[2]);
    cfg.max_iter = atoi(argv[3]);
    cfg.num_threads = atoi(argv[4]);

    if (cfg.largura <= 0 || cfg.altura <= 0 || cfg.max_iter <= 0 || cfg.num_threads <= 0) {
        fprintf(stderr, "Erro: Todos os argumentos devem ser numeros inteiros positivos maiores que zero.\n");
        return 1;
    }

    size_t total_pixels = (size_t)cfg.largura * cfg.altura;
    unsigned char *img = malloc(total_pixels * sizeof(unsigned char));
    if (!img) {
        fprintf(stderr, "Erro: Falha na alocacao de memoria para o buffer de imagem.\n");
        return 1;
    }

    double t0, t1;
    double t_serial, t_omp, t_p1, t_p2;

    t0 = get_time_sec();
    run_serial(cfg, img);
    t1 = get_time_sec();
    t_serial = t1 - t0;
    if (!salvar_pgm("mandelbrot_rpb_serial.pgm", img, cfg.largura, cfg.altura)) {
        free(img);
        return 1;
    }

    t0 = get_time_sec();
    run_openmp(cfg, img);
    t1 = get_time_sec();
    t_omp = t1 - t0;
    if (!salvar_pgm("mandelbrot_rpb_openmp.pgm", img, cfg.largura, cfg.altura)) {
        free(img);
        return 1;
    }

    t0 = get_time_sec();
    if (!run_pthreads_blocks(cfg, img)) {
        free(img);
        return 1;
    }
    t1 = get_time_sec();
    t_p1 = t1 - t0;
    if (!salvar_pgm("mandelbrot_rpb_pthreads1.pgm", img, cfg.largura, cfg.altura)) {
        free(img);
        return 1;
    }

    t0 = get_time_sec();
    if (!run_pthreads_cyclic(cfg, img)) {
        free(img);
        return 1;
    }
    t1 = get_time_sec();
    t_p2 = t1 - t0;
    if (!salvar_pgm("mandelbrot_rpb_pthreads2.pgm", img, cfg.largura, cfg.altura)) {
        free(img);
        return 1;
    }

    FILE *ft = fopen("times.txt", "w");
    if (!ft) {
        fprintf(stderr, "Erro ao criar arquivo times.txt\n");
        free(img);
        return 1;
    }

    fprintf(ft, "Serial: %.6f s\n", t_serial);
    fprintf(ft, "OpenMP: %.6f s\n", t_omp);
    fprintf(ft, "Pthreads1: %.6f s\n", t_p1);
    fprintf(ft, "Pthreads2: %.6f s\n", t_p2);
    fclose(ft);

    free(img);
    return 0;
}