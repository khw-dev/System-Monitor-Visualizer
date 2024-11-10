#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>
#include <curl/curl.h>

#define DISCORD_WEBHOOK_URL "디스코드 웹훅 URL"
#define MAX_RECORDS 100

double get_cpu_usage() {
    static FILETIME previous_idle_time, previous_kernel_time, previous_user_time;
    FILETIME idle_time, kernel_time, user_time;

    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        ULARGE_INTEGER prev_idle, prev_kernel, prev_user, idle, kernel, user;

        prev_idle.LowPart = previous_idle_time.dwLowDateTime;
        prev_idle.HighPart = previous_idle_time.dwHighDateTime;
        prev_kernel.LowPart = previous_kernel_time.dwLowDateTime;
        prev_kernel.HighPart = previous_kernel_time.dwHighDateTime;
        prev_user.LowPart = previous_user_time.dwLowDateTime;
        prev_user.HighPart = previous_user_time.dwHighDateTime;

        idle.LowPart = idle_time.dwLowDateTime;
        idle.HighPart = idle_time.dwHighDateTime;
        kernel.LowPart = kernel_time.dwLowDateTime;
        kernel.HighPart = kernel_time.dwHighDateTime;
        user.LowPart = user_time.dwLowDateTime;
        user.HighPart = user_time.dwHighDateTime;

        ULONGLONG idle_diff = idle.QuadPart - prev_idle.QuadPart;
        ULONGLONG kernel_diff = kernel.QuadPart - prev_kernel.QuadPart;
        ULONGLONG user_diff = user.QuadPart - prev_user.QuadPart;

        ULONGLONG total_time = kernel_diff + user_diff;
        if (total_time > 0) {
            double cpu_usage = 100.0 * (kernel_diff + user_diff - idle_diff) / total_time;

            previous_idle_time = idle_time;
            previous_kernel_time = kernel_time;
            previous_user_time = user_time;

            return cpu_usage;
        }
    }
    return 0.0;
}

double get_memory_usage() {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.dwMemoryLoad;
    }
    return 0.0;
}

void save_data_to_file(double cpu, double memory, int record_num) {
    FILE *file = fopen("usage_data.txt", "a");
    if (!file) {
        fprintf(stderr, "파일을 열 수 없습니다.\n");
        return;
    }
    fprintf(file, "%d %lf %lf\n", record_num, cpu, memory);
    fclose(file);
}

void generate_graph() {
    FILE *gp = popen("gnuplot", "w");
    if (!gp) {
        fprintf(stderr, "Gnuplot 실행 오류\n");
        return;
    }

    fprintf(gp, "set encoding utf8\n");
    fprintf(gp, "set term pngcairo size 800,600 font \"Sans,10\" \n");
    fprintf(gp, "set output 'usage_graph.png'\n");
    fprintf(gp, "set title 'CPU 및 메모리 사용률 변화'\n");
    fprintf(gp, "set xlabel '시간 (초)'\n");
    fprintf(gp, "set ylabel 'CPU 사용률 (%)'\n");
    fprintf(gp, "set zlabel '메모리 사용률 (%)'\n");
    fprintf(gp, "set dgrid3d 30,30\n");
    fprintf(gp, "set hidden3d\n");

    fprintf(gp, "splot 'usage_data.txt' using 1:2:3 with lines title 'CPU 및 메모리 사용률'\n");

    fflush(gp);
    pclose(gp);
}

int send_to_discord(double cpu_usage, double memory_usage) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: multipart/form-data");

    curl_easy_setopt(curl, CURLOPT_URL, DISCORD_WEBHOOK_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    curl_mime *form = curl_mime_init(curl);

    curl_mimepart *field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, "usage_graph.png");

    curl_mimepart *text_field = curl_mime_addpart(form);
    curl_mime_name(text_field, "payload_json");

    char json_payload[1024];
    snprintf(json_payload, sizeof(json_payload),
             "{\"embeds\":[{\"title\":\"시스템 사용률\","
             "\"description\":\"CPU 사용률 : %.2f%%\\n메모리 사용률 : %.2f%%\","
             "\"color\":16711680}]}",
             cpu_usage, memory_usage);

    curl_mime_data(text_field, json_payload, CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    curl_mime_free(form);
    curl_slist_free_all(headers);

    return (res == CURLE_OK) ? 0 : -1;
}

int main() {
    double cpu_usage, memory_usage;
    int record_count = 0;

    FILE *file = fopen("usage_data.txt", "w");
    if (file) fclose(file);

    while (record_count < MAX_RECORDS) {
        cpu_usage = get_cpu_usage();
        memory_usage = get_memory_usage();
        printf("CPU 사용률 : %.2f%%, 메모리 사용률 : %.2f%%\n", cpu_usage, memory_usage);

        save_data_to_file(cpu_usage, memory_usage, record_count);

        generate_graph();

        if (send_to_discord(cpu_usage, memory_usage) != 0) {
            fprintf(stderr, "Discord Webhook 전송에 실패했습니다.\n");
        }

        sleep(5);
        record_count++;
    }
    return 0;
}
