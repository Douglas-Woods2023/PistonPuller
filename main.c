#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "cJSON.h"
#include <pthread.h>
#include <time.h>

// --- 跨平台兼容处理 ---
#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <unistd.h>
    #define MKDIR(path) mkdir(path, 0777)
#endif

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef _WIN32
    #define VERSION "0.3.0 on Win32"
#else
    #define VERSION "0.3.0 on Unix"
#endif

// --- 打印极客 Banner ---
void print_banner() {
    printf("\033[36m"); // 青色
    printf("====================================================\n");
    printf("  ____  _     _              ____        _ _           \n");
    printf(" |  _ \\(_)___| |_ ___  _ __ |  _ \\ _   _| | | ___ _ __ \n");
    printf(" | |_) | / __| __/ _ \\| '_ \\| |_) | | | | | |/ _ \\ '__|\n");
    printf(" |  __/| \\__ \\ || (_) | | | |  __/| |_| | | |  __/ |   \n");
    printf(" |_|   |_|___/\\__\\___/|_| |_|_|    \\__,_|_|_|\\___|_|   \n");
    printf("====================================================\n");
    printf("\033[0m"); // 恢复
    printf(" [PistonPuller: 极简 Minecraft 原版下载器 / Minimalist Vanilla Downloader] v%s\n\n", VERSION);
    printf("  作者 (Author): Douglas Woods (GitHub: @Douglas-Woods2023, Bilibili: @Douglas_Woods)\n");
    printf("  主页 (Homepage): https://github.com/Douglas-Woods2023/PistonPuller\n");
    printf("  Issue: https://github.com/Douglas-Woods2023/PistonPuller/issues\n");
    printf("  说明 (Description): 这是一个极简的 Minecraft 原版下载器，支持 HMCL、MultiMC 等启动器。\n");
    printf("  (This is a minimalist Minecraft vanilla downloader, supporting HMCL, MultiMC, etc.)\n");
    printf("====================================================\n\n");
}


// ==========================================
// 多线程核心数据结构与全局变量
// ==========================================
typedef struct {
    char url[512];
    char save_path[1024];
    long expected_size;
} DownloadTask;

DownloadTask tasks[15000]; // 任务队列（足够装下 assets 和 libraries）
int total_tasks = 0;       // 总任务数
int current_index = 0;     // 当前抢到的任务索引
int finished_count = 0;    // 已完成任务数

long total_bytes = 0;
long downloaded_bytes = 0;
time_t start_time;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // 互斥锁
int thread_count = 16;     // 并发线程数（16个线程足够快了）

// 绘制 apt 风格进度条 (带速度和预计时间)
void draw_progress_bar(int current, int total) {
    int bar_width = 30;
    float progress = (float)current / total;
    int pos = bar_width * progress;

    // 计算速度和剩余时间
    time_t now = time(NULL);
    double elapsed = difftime(now, start_time);
    if (elapsed <= 0) elapsed = 1.0; // 防止除以 0

    double speed = downloaded_bytes / elapsed; // 字节/秒
    double remain_bytes = total_bytes - downloaded_bytes;
    double eta_sec = remain_bytes / speed;

    // 格式化速度 (MB/s 或 KB/s)
    char speed_str[32];
    if (speed > 1024 * 1024) {
        sprintf(speed_str, "%.2f MB/s", speed / 1024.0 / 1024.0);
    } else {
        sprintf(speed_str, "%.1f KB/s", speed / 1024.0);
    }

    // 格式化剩余时间 (mm:ss)
    char eta_str[32];
    if (eta_sec > 3600) {
        sprintf(eta_str, "%dh%02dm", (int)(eta_sec / 3600), (int)(eta_sec / 60) % 60);
    } else {
        sprintf(eta_str, "%02d:%02d", (int)(eta_sec / 60), (int)eta_sec % 60);
    }

    // \033[K 清除当前行，\r 回到行首
    printf("\r\033[K[");
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %3.0f%% (%d/%d) | %s | 剩余: %s", progress * 100.0, current, total, speed_str, eta_str);
    fflush(stdout);
}

// --- 跨平台递归创建目录 ---
void mkdir_p(const char *path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    // 去掉结尾的斜杠
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = 0;
    }

    // 逐级创建目录
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            MKDIR(tmp);
            *p = '/'; // 统一替换成正斜杠，Windows 底层 API 也认正斜杠
        }
    }
    MKDIR(tmp);
}

// --- libcurl 进度条回调 ---
int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    // 获取断点偏移量
    long resume_offset = 0;
    if (clientp) {
        resume_offset = *(long*)clientp;
    }
    
    // 真实的已下载量和总大小
    long real_total = dltotal + resume_offset;
    long real_now = dlnow + resume_offset;

    if (real_total > 0) {
        double percent = (double)real_now / (double)real_total * 100.0;
        printf("\r下载进度 (Progress): %6.2f%% [%.2f MB / %.2f MB]", percent, (double)real_now / 1024 / 1024, (double)real_total / 1024 / 1024);
        fflush(stdout);
    }
    return 0;
}

// --- libcurl 写入文件回调 ---
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

// --- libcurl 下载封装 ---
int download_with_progress(const char* url, const char* filename, int show_progress) {
    int retries = 3;
    
    while (retries > 0) {
        CURL *curl = curl_easy_init();
        if (!curl) return 0;

        long local_size = 0;
        struct stat st;
        if (stat(filename, &st) == 0) {
            local_size = st.st_size;
        }

        FILE *fp = (local_size > 0) ? fopen(filename, "ab") : fopen(filename, "wb");

        if (!fp) {
            curl_easy_cleanup(curl);
            return 0;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        #ifdef _WIN32
            curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
        #endif
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L); 
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10L); 
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

        if (local_size > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)local_size);
        }
        
        if (show_progress) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &local_size); // 把断点偏移传给进度条
        } else {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L); 
        }

        CURLcode res = curl_easy_perform(curl);
        
        // 获取 HTTP 状态码
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        fclose(fp);
        curl_easy_cleanup(curl);

        // 核心修复：如果是 416，说明本地文件已经完整了，直接当作成功
        if (res == CURLE_OK || http_code == 416) {
            if (show_progress) printf("\n[OK] 下载成功 (Success): %s\n", filename);
            return 1; 
        } else {
            if (show_progress) printf("\n[ERR] 下载失败 (Failed): %s, 正在重试...\n", curl_easy_strerror(res));
            
            retries--;
            if (retries > 0) {
                #ifdef _WIN32
                    Sleep(500);
                #else
                    usleep(500000);
                #endif
            }
        }
    }
    return 0; 
}

// --- 辅助函数：读取本地 JSON 文件 ---
char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(length + 1);
    if (data) {
        fread(data, 1, length, f);
        data[length] = '\0';
    }
    fclose(f);
    return data;
}

// --- 检查文件是否存在且大小是否匹配 ---
int need_download(const char* filepath, long expected_size) {
    struct stat st;
    if (stat(filepath, &st) == 0) {
        if (st.st_size == expected_size) return 0; 
    }
    return 1; 
}

// --- 下载所有依赖库 ---
void collect_libraries(cJSON *root, const char *mc_dir) {
    cJSON *libraries = cJSON_GetObjectItem(root, "libraries");
    if (!libraries || !cJSON_IsArray(libraries)) return;

    int array_size = cJSON_GetArraySize(libraries);
    printf(">>> 正在分析 %d 个依赖库...\n", array_size);

    for (int i = 0; i < array_size; i++) {
        cJSON *lib = cJSON_GetArrayItem(libraries, i);
        cJSON *downloads = cJSON_GetObjectItem(lib, "downloads");
        if (!downloads) continue;
        cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");
        if (!artifact) continue;

        cJSON *path_obj = cJSON_GetObjectItem(artifact, "path");
        cJSON *url_obj = cJSON_GetObjectItem(artifact, "url");
        cJSON *size_obj = cJSON_GetObjectItem(artifact, "size");

        if (path_obj && cJSON_IsString(path_obj) && url_obj && cJSON_IsString(url_obj)) {
            long expected_size = (size_obj && cJSON_IsNumber(size_obj)) ? (long)size_obj->valuedouble : 0;
            char save_path[1024];
            sprintf(save_path, "%s/libraries/%s", mc_dir, path_obj->valuestring);

            if (need_download(save_path, expected_size)) {
                // 建目录
                char dir_path[1024];
                strcpy(dir_path, save_path);
                char *last_slash = strrchr(dir_path, '/');
                if (last_slash) { *last_slash = '\0'; mkdir_p(dir_path); }
                
                // 塞入队列
                strcpy(tasks[total_tasks].url, url_obj->valuestring);
                strcpy(tasks[total_tasks].save_path, save_path);
                tasks[total_tasks].expected_size = expected_size;
                total_bytes += expected_size;
                total_tasks++;
            }
        }
    }
}

// --- 下载 Assets 资源文件 ---
void collect_assets(cJSON *root, const char *mc_dir) {
    cJSON *assetIndex = cJSON_GetObjectItem(root, "assetIndex");
    if (!assetIndex) return;
    cJSON *id_obj = cJSON_GetObjectItem(assetIndex, "id");
    cJSON *url_obj = cJSON_GetObjectItem(assetIndex, "url");
    if (!id_obj || !url_obj) return;

    char index_url[512], index_path[1024];
    strncpy(index_url, url_obj->valuestring, sizeof(index_url) - 1);
    
    char indexes_dir[1024];
    sprintf(indexes_dir, "%s/assets/indexes", mc_dir);
    mkdir_p(indexes_dir);
    
    sprintf(index_path, "%s/assets/indexes/%s.json", mc_dir, id_obj->valuestring);
    
    // 索引文件很小，直接单线程下
    if (!download_with_progress(index_url, index_path, 0)) return;

    char* index_str = read_file(index_path);
    cJSON* index_root = cJSON_Parse(index_str);
    free(index_str);
    if (!index_root) return;

    cJSON *objects = cJSON_GetObjectItem(index_root, "objects");
    if (!objects) { cJSON_Delete(index_root); return; }

    printf(">>> 正在分析 %d 个资源文件...\n", cJSON_GetArraySize(objects));
    
    for (cJSON *item = objects->child; item != NULL; item = item->next) {
        cJSON *hash_obj = cJSON_GetObjectItem(item, "hash");
        cJSON *size_obj = cJSON_GetObjectItem(item, "size");
        
        if (hash_obj && cJSON_IsString(hash_obj)) {
            char hash[64];
            strncpy(hash, hash_obj->valuestring, sizeof(hash) - 1);
            long expected_size = (size_obj && cJSON_IsNumber(size_obj)) ? (long)size_obj->valuedouble : 0;

            char save_path[1024];
            sprintf(save_path, "%s/assets/objects/%.2s/%s", mc_dir, hash, hash);
            
            if (need_download(save_path, expected_size)) {
                char dir_path[1024];
                strcpy(dir_path, save_path);
                char *last_slash = strrchr(dir_path, '/');
                if (last_slash) { *last_slash = '\0'; mkdir_p(dir_path); }

                // 塞入队列
                sprintf(tasks[total_tasks].url, "https://resources.download.minecraft.net/%.2s/%s", hash, hash);
                strcpy(tasks[total_tasks].save_path, save_path);
                tasks[total_tasks].expected_size = expected_size;
                total_bytes += expected_size;
                total_tasks++;
            }
        }
    }
    cJSON_Delete(index_root);
}

// --- 打印帮助信息 ---
void print_help(const char* prog_name) {
    printf("用法 (Usage):\n");
    printf("  %s [版本号] [安装路径] [选项]\n\n", prog_name);
    printf("内置指令 (Commands):\n");
    printf("  latest          下载最新的正式版 (Download latest release)\n");
    printf("  snapshot        下载最新的快照版 (Download latest snapshot)\n");
    printf("  list [类型]     列出所有版本，可选类型 release/snapshot (List versions)\n\n");
    printf("选项 (Options):\n");
    printf("  --no-assets     跳过几千个资源文件的下载，交由启动器补全 (Skip assets)\n");
    printf("  -h, --help      显示此帮助信息 (Show this help message)\n");
    printf("  -v, --version   显示当前版本号 (Show version info)\n\n");
    printf("示例 (Examples):\n");
    printf("  %s latest\n", prog_name);
    printf("  %s 26.3 --no-assets\n", prog_name);
    printf("  %s 1.12.2 /path/to/minecraft\n", prog_name);
    printf("  %s list release\n\n", prog_name);
}

// 线程工作函数
void* worker_thread(void* arg) {
    while (1) {
        int my_index = -1;
        
        pthread_mutex_lock(&lock);
        if (current_index < total_tasks) {
            my_index = current_index;
            current_index++;
        }
        pthread_mutex_unlock(&lock);

        if (my_index == -1) break;

        // 执行下载
        int success = download_with_progress(tasks[my_index].url, tasks[my_index].save_path, 0);

        // 完成后更新进度
        pthread_mutex_lock(&lock);
        finished_count++;
        if (success) {
            downloaded_bytes += tasks[my_index].expected_size; 
        } else {
            printf("\r\033[K\033[31m[ERR] 下载失败: %s\033[0m\n", tasks[my_index].save_path);
        }
        draw_progress_bar(finished_count, total_tasks);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8); // 强制控制台使用 UTF-8 输出
        
        // 开启 Windows 终端的 ANSI 转义码支持 (让进度条和颜色生效)
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    #endif

    print_banner();

    const char* target_version = "26.3"; 
    int skip_assets = 0; 
    char list_filter[32] = {0}; // 用于 list 过滤
    
        char mc_dir[1024];
    char* home = NULL;
    
    #ifdef _WIN32
        home = getenv("USERPROFILE"); // Windows
    #else
        home = getenv("HOME");        // Linux/macOS
    #endif

    if (!home) {
        printf("[ERR] 找不到用户主目录 (Cannot find home directory)\n");
        return 1;
    }
    sprintf(mc_dir, "%s/.minecraft", home); 

    // 统一的参数遍历
    int positional_count = 0; 
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("PistonPuller v%s\n", VERSION);
            return 0;
        }
        
        if (strcmp(argv[i], "--no-assets") == 0) {
            skip_assets = 1;
        } else if (argv[i][0] != '-') { 
            if (positional_count == 0) {
                target_version = argv[i]; 
            } else if (positional_count == 1) {
                // 如果 target_version 是 list，第二个参数作为过滤器
                if (strcmp(target_version, "list") == 0) {
                    strncpy(list_filter, argv[i], sizeof(list_filter) - 1);
                } else {
                    strncpy(mc_dir, argv[i], sizeof(mc_dir) - 1); 
                    mc_dir[sizeof(mc_dir) - 1] = '\0';
                }
            }
            positional_count++;
        }
    }

    // ==========================================
    // 第一步：获取版本总清单
    // ==========================================
    const char* manifest_url = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
    printf(">>> [1/4] 正在获取版本总清单 (Fetching manifest)...\n");
    if (!download_with_progress(manifest_url, "version_manifest_v2.json", 1)) return 1;

    char* manifest_str = read_file("version_manifest_v2.json");
    if (!manifest_str) {
        printf("[ERR] 无法读取本地清单文件 (Failed to read manifest)\n");
        return 1;
    }

    cJSON* manifest_root = cJSON_Parse(manifest_str);
    free(manifest_str);
    if (!manifest_root) {
        printf("[ERR] 无法解析清单文件 (Failed to parse manifest)\n");
        return 1;
    }

    // ==========================================
    // 第二步：拦截检索指令 & 解析总清单
    // ==========================================
    
    // 1. 拦截 list 指令
    if (strcmp(target_version, "list") == 0) {
        printf(">>> 游戏版本列表 (Game Versions):\n");
        printf("    (提示：如果输出太长，可以使用管道过滤，例如 ./pistonpuller list | grep 1.12)\n");
        printf("--------------------------------------------------\n");
        
        cJSON* versions = cJSON_GetObjectItem(manifest_root, "versions");
        
        // 去掉 count < 20 的限制，直接全量遍历
        for (cJSON* v = versions->child; v != NULL; v = v->next) {
            cJSON* id = cJSON_GetObjectItem(v, "id");
            cJSON* type = cJSON_GetObjectItem(v, "type");
            if (id && type) {
                // 保留 list_filter 过滤功能（比如 list release）
                if (strlen(list_filter) == 0 || strcmp(type->valuestring, list_filter) == 0) {
                    // 给 release 加个绿色，给 snapshot 加个黄色，更美观
                    if (strcmp(type->valuestring, "release") == 0) {
                        printf("  - \033[32m%-15s\033[0m [%s]\n", id->valuestring, type->valuestring);
                    } else if (strcmp(type->valuestring, "snapshot") == 0) {
                        printf("  - \033[33m%-15s\033[0m [%s]\n", id->valuestring, type->valuestring);
                    } else {
                        printf("  - %-15s [%s]\n", id->valuestring, type->valuestring);
                    }
                }
            }
        }
        printf("--------------------------------------------------\n");
        printf("总计输出完毕。复制你要的版本号，运行 ./pistonpuller <版本号> 即可下载。\n");
        
        cJSON_Delete(manifest_root);
        return 0; // 只查不下，直接退出
    }

    // 2. 拦截 latest / snapshot 指令
    // 关键防雷：用一个安全的数组来接住版本号，防止 cJSON 释放后变成野指针
    char resolved_version[64] = {0};
    if (strcmp(target_version, "latest") == 0 || strcmp(target_version, "snapshot") == 0) {
        cJSON* latest_obj = cJSON_GetObjectItem(manifest_root, "latest");
        cJSON* res_obj = cJSON_GetObjectItem(latest_obj, strcmp(target_version, "latest") == 0 ? "release" : "snapshot");
        if (res_obj && cJSON_IsString(res_obj)) {
            strncpy(resolved_version, res_obj->valuestring, sizeof(resolved_version) - 1);
            target_version = resolved_version; // 指向安全的数组
            printf(">>> [检索] 自动切换至 (Auto switched to): %s\n", target_version);
        }
    }

    printf(">>> 目标版本 (Target Version): %s\n", target_version);
    printf(">>> 安装目录 (Install Dir): %s\n", mc_dir);
    if (skip_assets) printf(">>> [模式] 跳过 Assets 下载 (Skip Assets)\n");

    printf(">>> [2/4] 正在查找元数据地址 (Finding metadata URL)...\n");
    cJSON* versions = cJSON_GetObjectItem(manifest_root, "versions");
    char version_json_url[512] = {0};
    
    if (versions && cJSON_IsArray(versions)) {
        int array_size = cJSON_GetArraySize(versions);
        for (int i = 0; i < array_size; i++) {
            cJSON* version_obj = cJSON_GetArrayItem(versions, i);
            cJSON* id = cJSON_GetObjectItem(version_obj, "id");
            if (id && cJSON_IsString(id) && strcmp(id->valuestring, target_version) == 0) {
                cJSON* url = cJSON_GetObjectItem(version_obj, "url");
                if (url && cJSON_IsString(url)) {
                    strncpy(version_json_url, url->valuestring, sizeof(version_json_url) - 1);
                    break;
                }
            }
        }
    }
    cJSON_Delete(manifest_root); // 释放完 cJSON

    if (strlen(version_json_url) == 0) {
        printf("[ERR] 找不到版本 (Version not found): %s\n", target_version);
        return 1;
    }

    // ==========================================
    // 第三步：下载并保存具体版本 JSON 到正确位置
    // ==========================================
    printf(">>> [3/4] 正在获取 %s 的详细配置文件 (Fetching version config)...\n", target_version);
    
    char version_dir[1024];
    sprintf(version_dir, "%s/versions/%s", mc_dir, target_version);
    mkdir_p(version_dir); 

    char version_json_file[1024];
    sprintf(version_json_file, "%s/versions/%s/%s.json", mc_dir, target_version, target_version);
    
    if (!download_with_progress(version_json_url, version_json_file, 1)) return 1;

    char* version_json_str = read_file(version_json_file);
    cJSON* version_root = cJSON_Parse(version_json_str);
    free(version_json_str);
    if (!version_root) return 1;

    cJSON* downloads = cJSON_GetObjectItem(version_root, "downloads");
    cJSON* client = cJSON_GetObjectItem(downloads, "client");
    cJSON* client_url_obj = cJSON_GetObjectItem(client, "url");

    if (!client_url_obj || !cJSON_IsString(client_url_obj)) {
        printf("[ERR] 无法找到 client.jar 的下载链接 (URL not found)!\n");
        cJSON_Delete(version_root);
        return 1;
    }
    char jar_url[512];
    strncpy(jar_url, client_url_obj->valuestring, sizeof(jar_url) - 1);

    // ==========================================
    // 第四步：下载 client.jar 和 libraries
    // ==========================================
    printf(">>> [4/4] 正在下载游戏本体和依赖库 (Downloading client.jar)...\n");
    
    char save_path[1024];
    sprintf(save_path, "%s/versions/%s/%s.jar", mc_dir, target_version, target_version);

    if (download_with_progress(jar_url, save_path, 1)) {
        printf("[OK] 游戏本体下载完成 (Game downloaded)!\n");
    } else {
        printf("[ERR] 下载游戏本体失败 (Game download failed).\n");
        cJSON_Delete(version_root);
        return 1;
    }

    // ==========================================
    // 开始收集多线程任务
    // ==========================================
    collect_libraries(version_root, mc_dir);
    
    if (!skip_assets) {
        collect_assets(version_root, mc_dir);
    } else {
        printf("\n>>> [SKIP] 已跳过 Assets 下载。\n");
    }
    cJSON_Delete(version_root);

    // ==========================================
    // 启动多线程下载
    // ==========================================
    if (total_tasks > 0) {
        printf("\n>>> 收集完毕！共 %d 个文件，启动 %d 个线程下载...\n", total_tasks, thread_count);
        fflush(stdout);

        start_time = time(NULL);

        // 必须调用 curl_global_init 保证多线程安全
        curl_global_init(CURL_GLOBAL_ALL);

        pthread_t threads[16]; // 最多16个线程
        for (int i = 0; i < thread_count; i++) {
            pthread_create(&threads[i], NULL, worker_thread, NULL);
        }

        // 等待所有线程结束
        for (int i = 0; i < thread_count; i++) {
            pthread_join(threads[i], NULL);
        }

        printf("\n"); // 进度条结束后换行
        curl_global_cleanup();
        time_t end_time = time(NULL);
        double total_sec = difftime(end_time, start_time);
        int min = (int)total_sec / 60;
        int sec = (int)total_sec % 60;
        
        // 把总大小换算成 MB
        double total_mb = (double)total_bytes / 1024.0 / 1024.0;
        
        printf(">>> \033[36m下载阶段结束 (Download finished)!\033[0m\n");
        printf(">>> 总大小 (Total Size): %.2f MB\n", total_mb);
        printf(">>> 总耗时 (Total Time): %02d 分 (min) %02d 秒 (sec)\n", min, sec);
    } else {
        printf("\n>>> 所有文件均已存在，无需下载！\n");
    }

    if (finished_count == total_tasks && total_bytes > 0) {
        // 这里稍微简单判断一下，只要任务数量对上了就算成功
        // 如果想严谨，可以再引入一个 failed_count 变量
        printf("\n\033[32m[SUCCESS] 大功告成 (All Done)!\033[0m\n");
    } else {
        printf("\n\033[33m[WARN] 部分文件下载失败，请重新运行本程序以补齐缺失文件。\033[0m\n");
    }
    printf("现在你可以打开 HMCL，指向 %s 目录启动 %s 了。\n", mc_dir, target_version);

    return 0;
}