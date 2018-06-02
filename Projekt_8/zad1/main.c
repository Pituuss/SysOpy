#define _DEFAULT_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>
#include <zconf.h>
#include <unistd.h>

const int buff_size = 2000000;

typedef struct transform_props {
  int begin;
  int end;
  int width;
  int heigth;
  int filter_size;
  int **pict_matrix;
  double **filter_matrix;
  int **out_pict_matrix;
} transform_props;

void save_time_res(clock_t r_time[2], struct tms tms_time[2], int threads,
                   int width, int heigth, int filter_size);
void *wrapped_apply(void *);
void load_matrix(char *buff, int **out_matrix, FILE *fp, int heigth, int width);
void free_matrix(int width, int heigth, int **matrix);
int **allocate_matrix(int width, int heigth);
void load_matrix_(char *buff, double **out_matrix, FILE *fp, int heigth,
                  int width);
void free_matrix_(int width, int heigth, double **matrix);
double **allocate_matrix_(int width, int heigth);
void save_picture(int width, int heigth, int **out_pict_matrix, FILE *fp);
void apply_filter(int begin, int end, int width, int heigth, int c,
                  int **pict_matrix, double **filter_matrix,
                  int **out_pict_matrix);
int calc_pixel(int x, int y, int width, int heigth, int c, int **I, double **K);

int main(int argc, char **argv) {
  if (argc < 5) {
    printf("threads count | in file path | filter path | out file name\n");
    exit(3);
  };
  char buff[buff_size];
  int threads;
  char *token, *string;
  int **pict_matrix;
  double **filter_matrix;
  int **out_pict_matrix;

  clock_t r_time[2] = {0, 0};
  struct tms tms_time[2];

  threads = strtol(argv[1], NULL, 10);

  FILE *fp_in_pict = fopen(argv[2], "r+");
  FILE *fp_filter = fopen(argv[3], "r+");
  FILE *fp_out_pict = fopen(argv[4], "w+");

  if (fp_in_pict == NULL) {
    perror("failed to open the picture");
    exit(5);
  }

  if (fp_filter == NULL) {
    perror("failed to open the filter");
    exit(5);
  }

  if (fp_out_pict == NULL) {
    perror("failed to create the output pciture");
    exit(5);
  }

  fgets(buff, buff_size, fp_in_pict); // get rid of "P2"
  fgets(buff, buff_size, fp_in_pict);

  string = strdup(buff);
  token = strsep(&string, " \t");
  int width = strtol(token, NULL, 10);
  token = strsep(&string, " \t");
  int heigth = strtol(token, NULL, 10);

  pict_matrix = allocate_matrix(width, heigth);
  out_pict_matrix = allocate_matrix(width, heigth);

  fgets(buff, buff_size, fp_in_pict); // get rid of max pixel val

  load_matrix(buff, pict_matrix, fp_in_pict, heigth, width);

  fgets(buff, buff_size, fp_filter);
  int filter_size = strtol(buff, NULL, 10);
  filter_matrix = allocate_matrix_(filter_size, filter_size);

  load_matrix_(buff, filter_matrix, fp_filter, filter_size, filter_size);

  pthread_t *thread = calloc(threads, sizeof(pthread_t));
  transform_props **props_table = malloc(threads * sizeof(transform_props *));

  r_time[0] = times(&tms_time[0]);

  for (int i = 0; i < threads; i++) {
    props_table[i] = malloc(sizeof(transform_props));
    props_table[i]->begin = i * width / threads;
    props_table[i]->end = (i + 1) * width / threads;
    props_table[i]->width = width;
    props_table[i]->heigth = heigth;
    props_table[i]->filter_size = filter_size;
    props_table[i]->pict_matrix = pict_matrix;
    props_table[i]->filter_matrix = filter_matrix;
    props_table[i]->out_pict_matrix = out_pict_matrix;
    pthread_create(&thread[i], NULL, wrapped_apply, (void *)props_table[i]);
  }

  for (int i = 0; i < threads; i++) {
    pthread_join(thread[i], NULL);
    free(props_table[i]);
  }
  free(props_table);

  r_time[1] = times(&tms_time[1]);

  save_picture(width, heigth, out_pict_matrix, fp_out_pict);
  save_time_res(r_time, tms_time, threads, width, heigth, filter_size);

  free_matrix(width, heigth, pict_matrix);
  free_matrix(width, heigth, out_pict_matrix);
  free_matrix_(filter_size, filter_size, filter_matrix);

  fclose(fp_in_pict);
  fclose(fp_out_pict);
  fclose(fp_filter);
}

void *wrapped_apply(void *props) {
  transform_props *unwrp_props = (transform_props *)props;
  apply_filter(unwrp_props->begin, unwrp_props->end, unwrp_props->width,
               unwrp_props->heigth, unwrp_props->filter_size,
               unwrp_props->pict_matrix, unwrp_props->filter_matrix,
               unwrp_props->out_pict_matrix);
  return (void *)0;
}

void apply_filter(int begin, int end, int width, int heigth, int c,
                  int **pict_matrix, double **filter_matrix,
                  int **out_pict_matrix) {
  for (int y = 0; y < heigth; y++)
    for (int x = begin; x < end; x++) {
      out_pict_matrix[y][x] =
          calc_pixel(x, y, width, heigth, c, pict_matrix, filter_matrix);
    }
}

int calc_pixel(int x, int y, int width, int heigth, int c, int **I,
               double **K) {
  double pixel_val = 0;
  for (int j = 0; j < c; j++) {
    int b = round(fmax(0, y - ceil(c / 2) + j));
    b = b < heigth ? b : heigth - 1;
    for (int i = 0; i < c; i++) {
      int a = round(fmax(0, x - ceil(c / 2) + i));
      a = a < width ? a : width - 1;
      double v = I[b][a] * K[j][i];
      pixel_val += v;
    }
  }
  pixel_val = pixel_val < 0 ? 0 : pixel_val;
  printf("pixel val: %lf\n", pixel_val);
  return round(pixel_val);
}

void save_picture(int width, int heigth, int **out_pict_matrix, FILE *fp) {
  char buff[1024];
  fprintf(fp, "P2\n");
  fprintf(fp, "%d %d\n", width, heigth);
  fprintf(fp, "%d\n", 255);
  for (int i = 0; i < heigth; i++) {
    for (int j = 0; j < width; j++) {

      if (j < width - 1) {
        sprintf(buff, "%d ", out_pict_matrix[i][j]);
      } else {
        sprintf(buff, "%d\n", out_pict_matrix[i][j]);
      }
      fputs(buff, fp);
    }
  }
}

int **allocate_matrix(int width, int heigth) {
  int **matrix = calloc(heigth, sizeof(int *));
  for (int i = 0; i < heigth; i++) {
    matrix[i] = calloc(width, sizeof(int));
  }
  return matrix;
}

void free_matrix(int width, int heigth, int **matrix) {
  for (int i = 0; i < heigth; i++) {
    free(matrix[i]);
  }
  free(matrix);
}

void load_matrix(char *buff, int **out_matrix, FILE *fp, int heigth,
                 int width) {
  int j = 0;
  int i = 0;
  int br = 1;
  while (fgets(buff, buff_size, fp) != NULL && br) {
    for (char *word = strtok(buff, " \n\r"); word != NULL;
         word = strtok(NULL, " \n\r")) {
      out_matrix[i][j] = strtol(word, NULL, 10);
      j++;
      i = (j < width) ? i : i + 1;
        if(i == heigth) {br = 0;break;}
      j = (j < width) ? j : 0;
    }
  }
}
double **allocate_matrix_(int width, int heigth) {
  double **matrix = calloc(heigth, sizeof(double *));
  for (int i = 0; i < heigth; i++) {
    matrix[i] = calloc(width, sizeof(double));
  }
  return matrix;
}

void free_matrix_(int width, int heigth, double **matrix) {
  for (int i = 0; i < heigth; i++) {
    free(matrix[i]);
  }
  free(matrix);
}

void load_matrix_(char *buff, double **out_matrix, FILE *fp, int heigth,
                  int width) {
  int j = 0;
  int i = 0;
  while (fgets(buff, buff_size, fp) != NULL) {
    for (char *word = strtok(buff, " \n\t\r"); word != NULL;
         word = strtok(NULL, " \t\n\r")) {
      out_matrix[i][j] = strtod(word, NULL);
      j++;
      i = (j < width) ? i : i + 1;
      j = (j < width) ? j : 0;
    }
  }
}

double time_diff(clock_t start, clock_t end) {
    return (double) (end - start) / sysconf(_SC_CLK_TCK);
}

void save_time_res(clock_t r_time[2], struct tms tms_time[2], int threads,
                   int width, int heigth, int filter_size) {
  FILE *fp = fopen("time_res.txt", "a");
  fprintf(fp, "Number of Threads: %d\n", threads);
  fprintf(fp, "Picture size: %dx%d\n", width, heigth);
  fprintf(fp, "Filter size: %dx%d\n", filter_size, filter_size);
  fprintf(fp, "Real:   %.2lf s   ", time_diff(r_time[0], r_time[1]));
  fprintf(fp, "User:   %.2lf s   ",
          time_diff(tms_time[0].tms_utime, tms_time[1].tms_utime));
  fprintf(fp, "System: %.2lf s\n",
          time_diff(tms_time[0].tms_stime, tms_time[1].tms_stime));
  fprintf(fp, "\n\n");
  fclose(fp);
}
