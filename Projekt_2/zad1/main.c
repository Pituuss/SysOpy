#include "lib_ver.h"
#include "sys_ver.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>

const int itrs = 10;
#define ops 4

double time_diff(clock_t t1, clock_t t2)
{
    return (double)(t2 - t1) / sysconf(_SC_CLK_TCK) / itrs;
}

void print_times(clock_t real_start, struct tms *time_start, clock_t real_end, struct tms *time_end, char *message, FILE *fp)
{
    printf("%s\n", message);
    printf("REAL: %fl   ", time_diff(real_start, real_end));
    printf("USER: %fl   ", time_diff(time_start->tms_utime, time_end->tms_utime));
    printf("SYSTEM: %fl     ", time_diff(time_start->tms_stime, time_end->tms_stime));
    printf("time per operation\n\n");

    fprintf(fp, "%s\n", message);
    fprintf(fp, "REAL: %fl  ", time_diff(real_start, real_end));
    fprintf(fp, "USER: %fl  ", time_diff(time_start->tms_utime, time_end->tms_utime));
    fprintf(fp, "SYSTEM: %fl    ", time_diff(time_start->tms_stime, time_end->tms_stime));
    fprintf(fp, "time per operation\n\n");
}

int main(int argc, char **argv)
{

    char *file_path;
    char *cp_to_path;
    int counter, block_size = 0, records_number = 0;
    while (1)
    {
        static struct option long_options[] =
            {
                {"copy", required_argument, 0, 'C'},
                {"path", required_argument, 0, 'P'},
                {"block_size", required_argument, 0, 's'},
                {"records_number", required_argument, 0, 'n'},
                {0, 0, 0, 0}};

        int option_index = 0;

        counter = getopt_long(argc, argv, ":F:n:s:C:",
                              long_options, &option_index);

        if (counter == -1)
            break;

        switch (counter)
        {
        case 0:
            break;

        case 'F':
            file_path = optarg;
            break;

        case 's':
            block_size = atoi(optarg);
            break;

        case 'n':
            records_number = atoi(optarg);
            break;

        case 'C':
            cp_to_path = optarg;
            break;

        case '?':
            printf("very helpfull info");
            break;

        default:
            abort();
        }
    }

    FILE *fp = fopen("times_res.out", "w+");

    clock_t time_arr[ops] = {0, 0, 0, 0};
    struct tms *tms_arr[ops];

    for (int i = 0; i < ops; i++)
        tms_arr[i] = calloc(1, sizeof(struct tms *));

    char **messages = calloc(ops, sizeof(char *));
    for (int i = 0; i < ops; i++)
        messages[i] = calloc(ops * 2 + 1, sizeof(char));

    int current_time = 0;
    int curretn_message = 0;

    time_arr[current_time] = times(tms_arr[current_time]);
    current_time += 1;

    strcpy(messages[curretn_message], "LIB_VER:");
    curretn_message += 2;
    for (int i = 0; i < itrs; i++)
    {
        // printf("%d\n",i);
        if (file_path && records_number && block_size)
        {
            lib_generate_file(file_path, records_number, block_size);
            // sys_generate_file(file_path, records_number, block_size);
        }
        if (file_path && cp_to_path && records_number && block_size)
        {
            lib_copy_file(file_path, cp_to_path, records_number, block_size);
            // sys_copy_file(file_path, cp_to_path, records_number, block_size);
        }
        if (file_path && records_number && block_size)
        {
            lib_sort_file(file_path, records_number, block_size);
            // sys_sort_file(file_path, records_number, block_size);
        }
    }

    time_arr[current_time] = times(tms_arr[current_time]);
    current_time += 1;

    strcpy(messages[curretn_message], "SYS_VER:");
    curretn_message += 2;

    time_arr[current_time] = times(tms_arr[current_time]);
    current_time += 1;

    for (int i = 0; i < itrs; i++)
    {
        // printf("%d\n",i);
        if (file_path && records_number && block_size)
        {
            // lib_generate_file(file_path, records_number, block_size);
            sys_generate_file(file_path, records_number, block_size);
        }
        if (file_path && cp_to_path && records_number && block_size)
        {
            // lib_copy_file(file_path, cp_to_path, records_number, block_size);
            sys_copy_file(file_path, cp_to_path, records_number, block_size);
        }
        if (file_path && records_number && block_size)
        {
            // lib_sort_file(file_path, records_number, block_size);
            sys_sort_file(file_path, records_number, block_size);
        }
    }

    time_arr[current_time] = times(tms_arr[current_time]);
    current_time += 1;

    fprintf(fp, "\tBLOCK_SIZE: %d RECORDS_NUMBER: %d\n\n", block_size, records_number);

    for (int i = 0; i < ops; i += 2)
        print_times(time_arr[i], tms_arr[i], time_arr[i + 1], tms_arr[i + 1], messages[i], fp);

    fclose(fp);
    return 0;
}