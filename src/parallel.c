// Author: APD team, except where source was noted

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
void init_contour_map(ppm_image **map, int thread_id, int nr_threads) {
    int start = thread_id * (double)CONTOUR_CONFIG_COUNT / nr_threads;
    int end = (thread_id + 1) * (double)CONTOUR_CONFIG_COUNT / nr_threads;
    if (end > CONTOUR_CONFIG_COUNT) end = CONTOUR_CONFIG_COUNT;

    for (int i = start; i < end; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

// Corresponds to step 1 of the marching squares algorithm, which focuses on sampling the image.
// Builds a p x q grid of points with values which can be either 0 or 1, depending on how the
// pixel values compare to the `sigma` reference value. The points are taken at equal distances
// in the original image, based on the `step_x` and `step_y` arguments.
void sample_grid(ppm_image *image, unsigned char **grid, int thread_id, int nr_threads, int step_x, int step_y, unsigned char sigma) {
    int p = image->x / step_x;
    int q = image->y / step_y;

    int start = thread_id * (double)p / nr_threads;
    int end = (thread_id + 1) * (double)p / nr_threads;
    if (end > p) end = p;

    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = image->data[i * step_x * image->y + j * step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > sigma) {
                grid[i][j] = 0;
            } else {
                grid[i][j] = 1;
            }
        }
    }

    // last sample points have no neighbors below / to the right, so we use pixels on the
    // last row / column of the input image for them
    for (int i = start; i < end; i++) {
        ppm_pixel curr_pixel = image->data[i * step_x * image->y + image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            grid[i][q] = 0;
        } else {
            grid[i][q] = 1;
        }
    }
    for (int j = 0; j < q; j++) {
        ppm_pixel curr_pixel = image->data[(image->x - 1) * image->y + j * step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            grid[p][j] = 0;
        } else {
            grid[p][j] = 1;
        }
    }
}

// Corresponds to step 2 of the marching squares algorithm, which focuses on identifying the
// type of contour which corresponds to each subgrid. It determines the binary value of each
// sample fragment of the original image and replaces the pixels in the original image with
// the pixels of the corresponding contour image accordingly.
void march(ppm_image *image, unsigned char **grid, ppm_image **contour_map, int thread_id, int nr_threads, int step_x, int step_y) {
    int p = image->x / step_x;
    int q = image->y / step_y;

    int start = thread_id * (double)p / nr_threads;
    int end = (thread_id + 1) * (double)p / nr_threads;
    if (end > p) end = p;

    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
            update_image(image, contour_map[k], i * step_x, j * step_y);
        }
    }
}

// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image *scaled_image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= scaled_image->x / step_x; i++) {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
    free(scaled_image->data);
    free(scaled_image);
}

void rescale_image(ppm_image *image, ppm_image **new_image, int thread_id, int nr_threads) {
    uint8_t sample[3];

    // we only rescale downwards
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
        *new_image = image;
        return;
    }

    int start = thread_id * (double)(*new_image)->x / nr_threads;
    int end = (thread_id + 1) * (double)(*new_image)->x / nr_threads;
    if (end > (*new_image)->x) end = (*new_image)->x;

    // use bicubic interpolation for scaling
    for (int i = start; i < end; i++) {
        for (int j = 0; j < (*new_image)->y; j++) {
            float u = (float)i / (float)((*new_image)->x - 1);
            float v = (float)j / (float)((*new_image)->y - 1);
            sample_bicubic(image, u, v, sample);

            (*new_image)->data[i * (*new_image)->y + j].red = sample[0];
            (*new_image)->data[i * (*new_image)->y + j].green = sample[1];
            (*new_image)->data[i * (*new_image)->y + j].blue = sample[2];
        }
    }
}

typedef struct
{
    int thread_id;
    int nr_threads;
    pthread_barrier_t *barrier;
    const char *filename;
    ppm_image *image;
    ppm_image **contour_map;
    ppm_image *scaled_image;
    unsigned char **grid;
} thread_arg;

void *thread_function(void *arg)
{
    thread_arg args = *(thread_arg *) arg;

    // 0. Initialize contour map
    init_contour_map(args.contour_map, args.thread_id, args.nr_threads);

    // 1. Rescale the image
    rescale_image(args.image, &args.scaled_image, args.thread_id, args.nr_threads);

    pthread_barrier_wait(args.barrier);

    // 2. Sample the grid
    sample_grid(args.scaled_image, args.grid, args.thread_id, args.nr_threads, STEP, STEP, SIGMA);

    pthread_barrier_wait(args.barrier);

    // 3. March the squares
    march(args.scaled_image, args.grid, args.contour_map, args.thread_id, args.nr_threads, STEP, STEP);

    pthread_barrier_wait(args.barrier);

    // 4. Write output
    if (args.thread_id == 0)
        write_ppm(args.scaled_image, args.filename);

    pthread_exit(NULL);
}

void init_resources(ppm_image ***contour_map, ppm_image **scaled_image, unsigned char ***grid)
{
    *contour_map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!contour_map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    *scaled_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!scaled_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    (*scaled_image)->x = RESCALE_X;
    (*scaled_image)->y = RESCALE_Y;
    (*scaled_image)->data = (ppm_pixel*)malloc((*scaled_image)->x * (*scaled_image)->y * sizeof(ppm_pixel));
    if (!scaled_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    int p = (*scaled_image)->x / STEP;
    int q = (*scaled_image)->y / STEP;
    *grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));
    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i <= p; i++) {
        (*grid)[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!(*grid)[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    ppm_image *image = read_ppm(argv[1]);

    int nr_threads = atoi(argv[3]);
    pthread_t threads[nr_threads];
    thread_arg args[nr_threads];
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, nr_threads);

    ppm_image **contour_map = NULL;
    ppm_image *scaled_image = NULL;
    unsigned char **grid = NULL;
    init_resources(&contour_map, &scaled_image, &grid);

    for (int i = 0; i < nr_threads; i++) {
        args[i].thread_id = i;
        args[i].nr_threads = nr_threads;
        args[i].barrier = &barrier;
        args[i].image = image;
        args[i].filename = argv[2];
        args[i].contour_map = contour_map;
        args[i].scaled_image = scaled_image;
        args[i].grid = grid;

        int r = pthread_create(&threads[i], NULL, thread_function, (void *) &args[i]);
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", i);
            exit(-1);
        }
    }

    for (int i = 0; i < nr_threads; i++) {
        void *status;
        int r = pthread_join(threads[i], &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }

    free_resources(image, scaled_image, contour_map, grid, STEP);
    pthread_barrier_destroy(&barrier);

    return 0;
}
