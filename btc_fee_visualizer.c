#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_HISTORY 72  // Guardar hasta 72 puntos (6 horas con actualizaciones cada 5 minutos)
#define CACHE_FILE "/tmp/btc_fee_cache.json"
#define MAX_SOURCES 3

// Estructura para un punto en el historial
typedef struct {
    double fastest;
    double halfHour;
    double hour;
    time_t timestamp;
} FeeHistoryPoint;

// Estructura para el historial de tarifas
typedef struct {
    FeeHistoryPoint *points;
    int size;
    int capacity;
    int current;
} FeeHistory;

// Estructura para almacenar datos de tarifas
typedef struct {
    // Tarifas actuales
    double fastestFee;      // Fastest fee (sat/vB)
    double halfHourFee;     // Half hour fee (sat/vB)
    double hourFee;         // Hour fee (sat/vB)
    
    // Datos de la red
    int blocks;             // Current block height
    double mempoolSizeMB;   // Mempool size in MB
    
    // Precios
    double btc_price_usd;   // Precio de BTC en USD
    double btc_price_eur;   // Precio de BTC en EUR
    
    // Historial
    FeeHistory history;     // Historial de tarifas
    
    time_t timestamp;       // Last update time
} FeeData;

// Variable global para controlar la visualización del historial
int show_history = 1;

// Estructura para fuentes de datos
typedef struct {
    const char *name;
    const char *fee_url;
    const char *mempool_url;
    const char *price_url;
} DataSource;

// Fuentes de datos disponibles
const DataSource data_sources[MAX_SOURCES] = {
    {
        "mempool.space",
        "https://mempool.space/api/v1/fees/recommended",
        "https://mempool.space/api/mempool",
        "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd,eur"
    },
    {
        "blockstream.info",
        "https://blockstream.info/api/fee-estimates",
        "https://blockstream.info/api/mempool",
        "https://blockchain.info/ticker"
    },
    {
        "bitcoinfees.earn.com",
        "https://bitcoinfees.earn.com/api/v1/fees/recommended",
        "https://bitcoinfees.earn.com/api/v1/fees/list",
        "https://api.coincap.io/v2/rates/bitcoin"
    }
};

int current_source = 0;  // Fuente de datos actual

// Estructura para el caché
typedef struct {
    FeeData data;
    time_t timestamp;
} CacheEntry;

// Declaraciones de funciones
void init_fee_history(FeeHistory *history, int capacity);
void add_to_history(FeeHistory *history, double fastest, double halfHour, double hour);
void draw_trend_graph(WINDOW *win, FeeHistory *history, int y, int x, int height, int width);
void draw_fee_visualization(FeeData *fee_data);

// Callback function for CURL to write response
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **response_ptr = (char **)userp;
    
    *response_ptr = realloc(*response_ptr, realsize + 1);
    if (*response_ptr == NULL) {
        return 0;
    }
    
    memcpy(*response_ptr, contents, realsize);
    (*response_ptr)[realsize] = '\0';
    
    return realsize;
}

// Obtener el precio de Bitcoin desde CoinGecko
int fetch_btc_price(FeeData *fee_data) {
    CURL *curl = curl_easy_init();
    char *response = NULL;
    int success = 0;
    
    if (curl) {
        // Usamos la API de CoinGecko para obtener precios
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd,eur");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        if (curl_easy_perform(curl) == CURLE_OK && response) {
            cJSON *json = cJSON_Parse(response);
            if (json) {
                cJSON *bitcoin = cJSON_GetObjectItemCaseSensitive(json, "bitcoin");
                if (bitcoin) {
                    cJSON *usd = cJSON_GetObjectItemCaseSensitive(bitcoin, "usd");
                    cJSON *eur = cJSON_GetObjectItemCaseSensitive(bitcoin, "eur");
                    
                    if (cJSON_IsNumber(usd) && cJSON_IsNumber(eur)) {
                        fee_data->btc_price_usd = usd->valuedouble;
                        fee_data->btc_price_eur = eur->valuedouble;
                        success = 1;
                    }
                }
                cJSON_Delete(json);
            }
        }
        
        if (response) free(response);
        curl_easy_cleanup(curl);
    }
    
    return success;
}

// Función para guardar en caché
int save_to_cache(const FeeData *data) {
    FILE *f = fopen(CACHE_FILE, "w");
    if (!f) return 0;
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "fastestFee", data->fastestFee);
    cJSON_AddNumberToObject(root, "halfHourFee", data->halfHourFee);
    cJSON_AddNumberToObject(root, "hourFee", data->hourFee);
    cJSON_AddNumberToObject(root, "blocks", data->blocks);
    cJSON_AddNumberToObject(root, "mempoolSizeMB", data->mempoolSizeMB);
    cJSON_AddNumberToObject(root, "btc_price_usd", data->btc_price_usd);
    cJSON_AddNumberToObject(root, "btc_price_eur", data->btc_price_eur);
    cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
    
    char *json_str = cJSON_Print(root);
    fputs(json_str, f);
    
    free(json_str);
    cJSON_Delete(root);
    fclose(f);
    return 1;
}

// Función para cargar desde caché
int load_from_cache(FeeData *data) {
    FILE *f = fopen(CACHE_FILE, "r");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *json_str = (char *)malloc(fsize + 1);
    if (fread(json_str, 1, fsize, f) != (size_t)fsize) {
        free(json_str);
        fclose(f);
        return 0;
    }
    json_str[fsize] = '\0';
    
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (!root) return 0;
    
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "fastestFee"))) data->fastestFee = item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "halfHourFee"))) data->halfHourFee = item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "hourFee"))) data->hourFee = item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "blocks"))) data->blocks = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "mempoolSizeMB"))) data->mempoolSizeMB = item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "btc_price_usd"))) data->btc_price_usd = item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "btc_price_eur"))) data->btc_price_eur = item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "timestamp"))) data->timestamp = (time_t)item->valuedouble;
    
    cJSON_Delete(root);
    return 1;
}

// Función para exportar datos actuales a CSV
void export_data_to_csv(const FeeData *data, const char *filename) {
    FILE *f = fopen(filename, "a");
    if (!f) return;
    
    // Escribir encabezados si el archivo está vacío
    if (ftell(f) == 0) {
        fprintf(f, "timestamp,fastest_fee,half_hour_fee,hour_fee,blocks,mempool_mb,btc_usd,btc_eur\n");
    }
    
    // Obtener la hora local
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&data->timestamp));
    
    // Escribir los datos
    fprintf(f, "\"%s\",%.1f,%.1f,%.1f,%d,%.2f,%.2f,%.2f\n",
            time_str,
            data->fastestFee,
            data->halfHourFee,
            data->hourFee,
            data->blocks,
            data->mempoolSizeMB,
            data->btc_price_usd,
            data->btc_price_eur);
    
    fclose(f);
}

// Función para exportar historial a CSV
void export_history_to_csv(const FeeHistory *history, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "timestamp,fastest_fee,half_hour_fee,hour_fee\n");
    
    for (int i = 0; i < history->size; i++) {
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&history->points[i].timestamp));
        
        fprintf(f, "\"%s\",%.1f,%.1f,%.1f\n",
                time_str,
                history->points[i].fastest,
                history->points[i].halfHour,
                history->points[i].hour);
    }
    
    fclose(f);
}

// Función para obtener datos de una fuente específica
int fetch_from_source(const DataSource *source, FeeData *fee_data) {
    CURL *curl = curl_easy_init();
    char *response = NULL;
    int success = 0;
    
    if (!curl) {
        fprintf(stderr, "Error al inicializar CURL\n");
        return 0;
    }
    
    fee_data->timestamp = time(NULL);
    
    // Obtener tarifas recomendadas
    curl_easy_setopt(curl, CURLOPT_URL, source->fee_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    if (curl_easy_perform(curl) == CURLE_OK && response) {
        cJSON *json = cJSON_Parse(response);
        if (json) {
            // Manejar diferentes formatos de respuesta
            cJSON *fastest = cJSON_GetObjectItemCaseSensitive(json, "fastestFee");
            cJSON *halfHour = cJSON_GetObjectItemCaseSensitive(json, "halfHourFee");
            cJSON *hour = cJSON_GetObjectItemCaseSensitive(json, "hourFee");
            
            // Si no encontramos los campos, puede que estén en otro formato
            if (!fastest) fastest = cJSON_GetObjectItemCaseSensitive(json, "2"); // blockstream usa números
            if (!halfHour) halfHour = cJSON_GetObjectItemCaseSensitive(json, "6");
            if (!hour) hour = cJSON_GetObjectItemCaseSensitive(json, "144");
            
            if (cJSON_IsNumber(fastest) && cJSON_IsNumber(halfHour) && cJSON_IsNumber(hour)) {
                fee_data->fastestFee = fastest->valuedouble;
                fee_data->halfHourFee = halfHour->valuedouble;
                fee_data->hourFee = hour->valuedouble;
                success = 1;
            }
            cJSON_Delete(json);
        }
        free(response);
        response = NULL;
    }
    
    // Obtener información del mempool
    if (success && source->mempool_url) {
        curl_easy_setopt(curl, CURLOPT_URL, source->mempool_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        if (curl_easy_perform(curl) == CURLE_OK && response) {
            cJSON *json = cJSON_Parse(response);
            if (json) {
                // Manejar diferentes formatos de respuesta
                cJSON *count = cJSON_GetObjectItemCaseSensitive(json, "count");
                cJSON *vsize = cJSON_GetObjectItemCaseSensitive(json, "vsize");
                
                if (!count) count = cJSON_GetObjectItemCaseSensitive(json, "n_tx");
                if (!vsize) vsize = cJSON_GetObjectItemCaseSensitive(json, "vsize");
                
                if (cJSON_IsNumber(count)) {
                    fee_data->blocks = count->valueint;
                }
                if (cJSON_IsNumber(vsize)) {
                    fee_data->mempoolSizeMB = vsize->valueint / 1000000.0; // Convertir a MB
                }
                cJSON_Delete(json);
            }
            free(response);
            response = NULL;
        }
    }
    
    // Obtener precio de Bitcoin
    if (success && source->price_url) {
        fetch_btc_price(fee_data); // La función ya maneja su propia lógica de fuentes
    }
    
    // Si todo salió bien, guardar en caché
    if (success) {
        save_to_cache(fee_data);
    }
    
    curl_easy_cleanup(curl);
    return success;
}

// Función para obtener datos, con reintentos y caché
int fetch_fee_data(FeeData *fee_data) {
    int attempts = 0;
    int success = 0;
    
    // Primero intentar cargar desde caché
    if (load_from_cache(fee_data)) {
        time_t now = time(NULL);
        // Si los datos en caché tienen menos de 5 minutos, usarlos
        if (difftime(now, fee_data->timestamp) < 300) {
            return 1;
        }
    }
    
    // Intentar con cada fuente hasta que una funcione
    while (attempts < MAX_SOURCES && !success) {
        current_source = (current_source + 1) % MAX_SOURCES;
        success = fetch_from_source(&data_sources[current_source], fee_data);
        attempts++;
    }
    
    return success;
}

// Cambiar a la siguiente fuente de datos
void cycle_data_source() {
    current_source = (current_source + 1) % MAX_SOURCES;
    // Forzar actualización con la nueva fuente
    FeeData fd;
    if (fetch_fee_data(&fd)) {
        // Actualizar la visualización
        draw_fee_visualization(&fd);
    }
}

// Initialize ncurses
void init_screen() {
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    // Initialize color pairs
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
}

// Draw the fee visualization
void draw_fee_visualization(FeeData *fee_data) {
    clear();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Draw header
    attron(A_BOLD | COLOR_PAIR(4));
    mvprintw(1, (max_x - 30) / 2, "BITCOIN TRANSACTION FEES");
    
    // Draw additional info
    mvprintw(2, 2, "Fuente: mempool.space | Tiempo real");
    attroff(A_BOLD | COLOR_PAIR(4));
    
    // Draw last update time and mempool info
    char time_str[64];
    strftime(time_str, sizeof(time_str), "Actualizado: %H:%M:%S", localtime(&fee_data->timestamp));
    mvprintw(4, 2, "%s", time_str);
    
    // Display mempool info if available
    if (fee_data->blocks > 0) {
        mvprintw(4, max_x - 20, "Bloque: %d", fee_data->blocks);
    }
    if (fee_data->mempoolSizeMB > 0) {
        mvprintw(5, 2, "Mempool: %.2f MB", fee_data->mempoolSizeMB);
    }
    
    // Draw separator
    mvhline(6, 0, '-', max_x);
    
    // Calculate bar lengths (max width - 30 for text)
    int max_bar_width = max_x - 35;
    
    // Find max fee for scaling
    double max_fee = fee_data->fastestFee;
    if (fee_data->halfHourFee > max_fee) max_fee = fee_data->halfHourFee;
    if (fee_data->hourFee > max_fee) max_fee = fee_data->hourFee;
    max_fee = max_fee * 1.2; // Add 20% padding
    
    // Calculate bar widths
    int fastest_width = (fee_data->fastestFee / max_fee) * max_bar_width;
    int half_hour_width = (fee_data->halfHourFee / max_fee) * max_bar_width;
    int hour_width = (fee_data->hourFee / max_fee) * max_bar_width;
    
    // Ensure minimum width for visibility
    fastest_width = fastest_width < 1 ? 1 : fastest_width;
    half_hour_width = half_hour_width < 1 ? 1 : half_hour_width;
    hour_width = hour_width < 1 ? 1 : hour_width;
    
    // Draw bars
    int y = 8;
    
    // Fastest fee (10 min)
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(y, 2, "RÁPIDO (10 min):");
    for (int i = 0; i < fastest_width; i++) {
        mvaddch(y, 25 + i, ' ' | A_REVERSE);
    }
    mvprintw(y, 25 + max_bar_width + 2, "%.1f sat/vB", fee_data->fastestFee);
    
    // Half hour fee
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(y + 2, 2, "MEDIO (30 min):");
    for (int i = 0; i < half_hour_width; i++) {
        mvaddch(y + 2, 25 + i, ' ' | A_REVERSE);
    }
    mvprintw(y + 2, 25 + max_bar_width + 2, "%.1f sat/vB", fee_data->halfHourFee);
    
    // Hour fee
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y + 4, 2, "LENTO (60 min):");
    for (int i = 0; i < hour_width; i++) {
        mvaddch(y + 4, 25 + i, ' ' | A_REVERSE);
    }
    mvprintw(y + 4, 25 + max_bar_width + 2, "%.1f sat/vB", fee_data->hourFee);
    
    // Draw scale
    attron(COLOR_PAIR(4) | A_DIM);
    mvprintw(y + 6, 25, "0");
    mvprintw(y + 6, 25 + (max_bar_width / 2), "%.0f", max_fee / 2);
    mvprintw(y + 6, 25 + max_bar_width - 3, "%.0f", max_fee);
    
    // Mostrar precios de Bitcoin
    attron(COLOR_PAIR(4) | A_BOLD);
    if (fee_data->btc_price_usd > 0) {
        mvprintw(y + 8, 2, "Precio BTC: $%.2f USD | %.2f EUR", 
                fee_data->btc_price_usd, 
                fee_data->btc_price_eur);
        
        // Calcular y mostrar el costo en USD para una transacción típica (vsize = 250 bytes)
        double avg_fee_sat = (fee_data->fastestFee + fee_data->halfHourFee) / 2.0;
        double fee_btc = (avg_fee_sat * 250) / 100000000.0; // Convertir a BTC
        double fee_usd = fee_btc * fee_data->btc_price_usd;
        double fee_eur = fee_btc * fee_data->btc_price_eur;
        
        mvprintw(y + 9, 2, "Costo estimado (250vB): $%.2f USD | %.2f EUR", fee_usd, fee_eur);
    }
    
    // Dibujar gráfico de tendencia si hay suficiente historial y está habilitado
    if (show_history && fee_data->history.size > 1) {
        int graph_y = y + 11;
        int graph_height = 10;
        if (graph_y + graph_height < max_y - 6) {  // Asegurar que haya espacio
            mvprintw(graph_y - 1, 2, "Tendencia de tarifas (últimas horas):");
            draw_trend_graph(stdscr, &fee_data->history, graph_y, 10, graph_height, max_x - 20);
        }
    }
    
    // Draw footer
    attron(COLOR_PAIR(4) | A_DIM);
    mvprintw(max_y - 2, 2, "q:Salir   r:Actualizar   ↑↓:Ajustar intervalo   h:Alternar historial");
    
    // Add some help text
    mvprintw(max_y - 4, 2, "Las tarifas están en satoshis por vbyte (sat/vB)");
    
    refresh();
}

// Inicializar el historial de tarifas
void init_fee_history(FeeHistory *history, int capacity) {
    history->points = (FeeHistoryPoint *)malloc(capacity * sizeof(FeeHistoryPoint));
    history->size = 0;
    history->capacity = capacity;
    history->current = 0;
}

// Agregar un punto al historial
void add_to_history(FeeHistory *history, double fastest, double halfHour, double hour) {
    if (history->size < history->capacity) {
        int index = history->size++;
        history->points[index].fastest = fastest;
        history->points[index].halfHour = halfHour;
        history->points[index].hour = hour;
        history->points[index].timestamp = time(NULL);
    } else {
        // Usar el índice actual para sobrescribir el punto más antiguo
        int index = history->current;
        history->points[index].fastest = fastest;
        history->points[index].halfHour = halfHour;
        history->points[index].hour = hour;
        history->points[index].timestamp = time(NULL);
        history->current = (history->current + 1) % history->capacity;
    }
}

// Dibujar el gráfico de tendencia
void draw_trend_graph(WINDOW *win, FeeHistory *history, int y, int x, int height, int width) {
    if (history->size < 2) return;
    
    // Encontrar el valor máximo para escalar el gráfico
    double max_fee = 0;
    for (int i = 0; i < history->size; i++) {
        if (history->points[i].fastest > max_fee) max_fee = history->points[i].fastest;
        if (history->points[i].halfHour > max_fee) max_fee = history->points[i].halfHour;
        if (history->points[i].hour > max_fee) max_fee = history->points[i].hour;
    }
    
    if (max_fee <= 0) max_fee = 1;  // Evitar división por cero
    
    // Dibujar el gráfico
    for (int t = 0; t < 3; t++) {
        int color_pair = t + 1;
        wattron(win, COLOR_PAIR(color_pair));
        
        for (int i = 0; i < width - 1 && i < history->size - 1; i++) {
            int idx1 = (history->current - i - 1 + history->capacity) % history->capacity;
            int idx2 = (history->current - i - 2 + history->capacity) % history->capacity;
            
            if (idx1 < 0 || idx2 < 0 || idx1 >= history->size || idx2 >= history->size) continue;
            
            double fee1 = (t == 0) ? history->points[idx1].fastest :
                         (t == 1) ? history->points[idx1].halfHour :
                                   history->points[idx1].hour;
                                   
            double fee2 = (t == 0) ? history->points[idx2].fastest :
                         (t == 1) ? history->points[idx2].halfHour :
                                   history->points[idx2].hour;
            
            int y1 = y + height - 1 - (int)((fee1 / max_fee) * (height - 2));
            int y2 = y + height - 1 - (int)((fee2 / max_fee) * (height - 2));
            
            // Asegurarse de que los valores estén dentro de los límites
            y1 = (y1 < y) ? y : (y1 >= y + height) ? y + height - 1 : y1;
            y2 = (y2 < y) ? y : (y2 >= y + height) ? y + height - 1 : y2;
            
            mvwaddch(win, y1, x + width - 2 - i, ACS_CKBOARD);
            
            // Dibujar línea entre puntos
            if (y1 != y2) {
                int step = (y2 > y1) ? 1 : -1;
                for (int py = y1; py != y2; py += step) {
                    mvwaddch(win, py, x + width - 2 - i, ACS_VLINE);
                }
            }
        }
        
        wattroff(win, COLOR_PAIR(color_pair));
    }
    
    // Leyenda
    wattron(win, COLOR_PAIR(3));
    mvwprintw(win, y, x + 5, "F: Rápido");
    wattron(win, COLOR_PAIR(2));
    mvwprintw(win, y, x + 15, "M: Medio");
    wattron(win, COLOR_PAIR(1));
    mvwprintw(win, y, x + 25, "L: Lento");
    wattroff(win, COLOR_PAIR(1));
}

int main() {
    // Initialize ncurses
    init_screen();
    
    FeeData current_fees = {0};
    init_fee_history(&current_fees.history, 60); // Mantener 60 puntos de historial (1 por minuto)
    int ch;
    time_t last_update = 0;
    const int UPDATE_INTERVAL = 30; // segundos entre actualizaciones
    int seconds_until_update = 0;
    
    // Initial fetch
    if (!fetch_fee_data(&current_fees)) {
        endwin();
        fprintf(stderr, "Error al obtener los datos de tarifas.\n");
        return 1;
    }
    last_update = time(NULL);
    
    // Main loop
    while (1) {
        time_t now = time(NULL);
        seconds_until_update = UPDATE_INTERVAL - (now - last_update);
        
        // Actualizar si ha pasado el intervalo
        if (seconds_until_update <= 0) {
            if (fetch_fee_data(&current_fees)) {
                // Agregar al historial
                if (show_history) {
                    add_to_history(&current_fees.history, 
                                 current_fees.fastestFee, 
                                 current_fees.halfHourFee, 
                                 current_fees.hourFee);
                }
                // Exportar automáticamente a CSV
                export_data_to_csv(&current_fees, "btc_fees_log.csv");
                last_update = now;
                seconds_until_update = UPDATE_INTERVAL;
            } else {
                // Si falla, reintentar en 5 segundos
                seconds_until_update = 5;
            }
        }
        
        // Actualizar la pantalla con el contador
        draw_fee_visualization(&current_fees);
        
        // Mostrar información de la fuente actual
        int max_y = getmaxy(stdscr);
        mvprintw(3, 2, "Fuente: %s | ", data_sources[current_source].name);
        printw("Tiempo real");
        
        // Mostrar contador de actualización
        mvprintw(max_y - 3, 2, "Próxima actualización: %d segundos ", seconds_until_update);
        
        // Mostrar controles
        mvprintw(max_y - 2, 2, "q:Salir   r:Actualizar   ↑↓:Ajustar intervalo   h:Alternar historial   s:Cambiar fuente   e:Exportar");
        
        // Esperar entrada del usuario con timeout (no bloqueante)
        nodelay(stdscr, TRUE);
        ch = getch();
        
        if (ch == 'q' || ch == 'Q') {
            break;
        } else if (ch == 'r' || ch == 'R' || ch == KEY_RESIZE) {
            // Actualizar manualmente o redimensionar ventana
            if (fetch_fee_data(&current_fees)) {
                if (show_history) {
                    add_to_history(&current_fees.history, 
                                 current_fees.fastestFee, 
                                 current_fees.halfHourFee, 
                                 current_fees.hourFee);
                }
                last_update = time(NULL);
            }
        } else if (ch == 'h' || ch == 'H') {
            // Alternar visualización del historial
            show_history = !show_history;
            clear();
        } else if (ch == 's' || ch == 'S') {
            // Cambiar a la siguiente fuente de datos
            cycle_data_source();
            // Actualizar la pantalla para mostrar la nueva fuente
            continue;
        } else if (ch == 'e' || ch == 'E') {
            // Exportar datos
            char filename[256];
            time_t now = time(NULL);
            struct tm *tm_now = localtime(&now);
            strftime(filename, sizeof(filename), "btc_fees_export_%Y%m%d_%H%M%S.csv", tm_now);
            
            // Exportar historial
            export_history_to_csv(&current_fees.history, filename);
            
            // Mostrar mensaje de confirmación
            mvprintw(max_y - 4, 2, "Datos exportados a %s", filename);
            refresh();
            napms(2000); // Mostrar mensaje por 2 segundos
        }
        
        // Esperar 1 segundo antes de la siguiente iteración
        sleep(1);
    }
    
    // Clean up
    endwin();
    return 0;
}
