#define _GNU_SOURCE

#include <gtk/gtk.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <libnotify/notify.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include "chart_utils.h"
#include "ui_utils.h"

// Forward declarations
static gboolean update_data(gpointer user_data);
static gpointer update_data_thread(gpointer user_data);
static void update_ui(gpointer user_data);

// Global application data structure
typedef struct {
    double fastest_fee;
    double half_hour_fee;
    double hour_fee;
    double economy_fee;
    double minimum_fee;
    double btc_price_usd;
    double btc_price_eur;
    double price_change_24h;
    int mempool_tx_count;
    int mempool_size_bytes;
    double mempool_total_fee;
    double mempool_avg_fee;
    
    // UI components
    AppUI *ui;
    
    // For auto-update
    guint update_timeout_id;
    int update_interval;  // in seconds
    
    // Thread control
    gboolean is_updating;
    pthread_t update_thread;
    
    // Database
    sqlite3 *db;
    
    // Mutex for thread safety
    pthread_mutex_t data_mutex;
} AppData;

static AppData app_data;

// Callback for CURL to write response
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    GString *response = (GString *)userp;
    g_string_append_len(response, contents, realsize);
    return realsize;
}

// Database initialization
gboolean init_database() {
    char *home_dir = g_get_home_dir();
    char *db_path = g_build_filename(home_dir, ".local", "share", "btc-fee-tracker", "data.db", NULL);
    
    // Create directory if it doesn't exist
    char *db_dir = g_path_get_dirname(db_path);
    if (g_mkdir_with_parents(db_dir, 0755) == -1) {
        g_warning("Failed to create database directory: %s", db_dir);
        g_free(db_dir);
        g_free(db_path);
        return FALSE;
    }
    g_free(db_dir);
    
    // Open database
    if (sqlite3_open(db_path, &app_data.db) != SQLITE_OK) {
        g_warning("Failed to open database: %s", sqlite3_errmsg(app_data.db));
        g_free(db_path);
        return FALSE;
    }
    
    // Create tables if they don't exist
    const char *sql = "CREATE TABLE IF NOT EXISTS fee_history ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "timestamp INTEGER NOT NULL,"
                     "fastest_fee REAL NOT NULL,"
                     "half_hour_fee REAL NOT NULL,"
                     "hour_fee REAL NOT NULL,"
                     "economy_fee REAL NOT NULL,"
                     "minimum_fee REAL NOT NULL"
                     ");";
                     
    char *err_msg = NULL;
    if (sqlite3_exec(app_data.db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        g_warning("Failed to create table: %s", err_msg);
        sqlite3_free(err_msg);
        g_free(db_path);
        return FALSE;
    }
    
    g_free(db_path);
    return TRUE;
}

// Save fee data to database
gboolean save_fee_data_to_db() {
    if (!app_data.db) return FALSE;
    
    const char *sql = "INSERT INTO fee_history (timestamp, fastest_fee, half_hour_fee, hour_fee, economy_fee, minimum_fee) "
                     "VALUES (?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app_data.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("Failed to prepare statement: %s", sqlite3_errmsg(app_data.db));
        return FALSE;
    }
    
    time_t now = time(NULL);
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_double(stmt, 2, app_data.fastest_fee);
    sqlite3_bind_double(stmt, 3, app_data.half_hour_fee);
    sqlite3_bind_double(stmt, 4, app_data.hour_fee);
    sqlite3_bind_double(stmt, 5, app_data.economy_fee);
    sqlite3_bind_double(stmt, 6, app_data.minimum_fee);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        g_warning("Failed to insert data: %s", sqlite3_errmsg(app_data.db));
        sqlite3_finalize(stmt);
        return FALSE;
    }
    
    sqlite3_finalize(stmt);
    return TRUE;
}

// Fetch current fee data from mempool.space API
gboolean fetch_fee_data() {
    CURL *curl = curl_easy_init();
    if (!curl) {
        g_warning("Failed to initialize CURL");
        return FALSE;
    }
    
    GString *response = g_string_new("");
    curl_easy_setopt(curl, CURLOPT_URL, "https://mempool.space/api/v1/fees/recommended");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BitcoinFeeTracker/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        g_warning("Failed to fetch fee data: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        g_string_free(response, TRUE);
        return FALSE;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response->str);
    if (!json) {
        g_warning("Failed to parse JSON response");
        curl_easy_cleanup(curl);
        g_string_free(response, TRUE);
        return FALSE;
    }
    
    // Extract fee data
    pthread_mutex_lock(&app_data.data_mutex);
    
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "fastestFee");
    if (cJSON_IsNumber(item)) app_data.fastest_fee = item->valuedouble;
    
    item = cJSON_GetObjectItemCaseSensitive(json, "halfHourFee");
    if (cJSON_IsNumber(item)) app_data.half_hour_fee = item->valuedouble;
    
    item = cJSON_GetObjectItemCaseSensitive(json, "hourFee");
    if (cJSON_IsNumber(item)) app_data.hour_fee = item->valuedouble;
    
    item = cJSON_GetObjectItemCaseSensitive(json, "economyFee");
    if (cJSON_IsNumber(item)) app_data.economy_fee = item->valuedouble;
    
    item = cJSON_GetObjectItemCaseSensitive(json, "minimumFee");
    if (cJSON_IsNumber(item)) app_data.minimum_fee = item->valuedouble;
    
    pthread_mutex_unlock(&app_data.data_mutex);
    
    // Save to database
    save_fee_data_to_db();
    
    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    g_string_free(response, TRUE);
    
    return TRUE;
}

// Fetch Bitcoin price data
gboolean fetch_btc_price() {
    CURL *curl = curl_easy_init();
    if (!curl) {
        g_warning("Failed to initialize CURL");
        return FALSE;
    }
    
    GString *response = g_string_new("");
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd,eur&include_24hr_change=true");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BitcoinFeeTracker/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        g_warning("Failed to fetch price data: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        g_string_free(response, TRUE);
        return FALSE;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response->str);
    if (!json) {
        g_warning("Failed to parse JSON response");
        curl_easy_cleanup(curl);
        g_string_free(response, TRUE);
        return FALSE;
    }
    
    // Extract price data
    pthread_mutex_lock(&app_data.data_mutex);
    
    cJSON *bitcoin = cJSON_GetObjectItemCaseSensitive(json, "bitcoin");
    if (bitcoin) {
        cJSON *usd = cJSON_GetObjectItemCaseSensitive(bitcoin, "usd");
        cJSON *eur = cJSON_GetObjectItemCaseSensitive(bitcoin, "eur");
        cJSON *change = cJSON_GetObjectItemCaseSensitive(bitcoin, "usd_24h_change");
        
        if (cJSON_IsNumber(usd)) app_data.btc_price_usd = usd->valuedouble;
        if (cJSON_IsNumber(eur)) app_data.btc_price_eur = eur->valuedouble;
        if (cJSON_IsNumber(change)) app_data.price_change_24h = change->valuedouble;
    }
    
    pthread_mutex_unlock(&app_data.data_mutex);
    
    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    g_string_free(response, TRUE);
    
    return TRUE;
}

// Fetch mempool data
gboolean fetch_mempool_data() {
    CURL *curl = curl_easy_init();
    if (!curl) {
        g_warning("Failed to initialize CURL");
        return FALSE;
    }
    
    GString *response = g_string_new("");
    curl_easy_setopt(curl, CURLOPT_URL, "https://mempool.space/api/mempool");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BitcoinFeeTracker/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        g_warning("Failed to fetch mempool data: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        g_string_free(response, TRUE);
        return FALSE;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response->str);
    if (!json) {
        g_warning("Failed to parse JSON response");
        curl_easy_cleanup(curl);
        g_string_free(response, TRUE);
        return FALSE;
    }
    
    // Extract mempool data
    pthread_mutex_lock(&app_data.data_mutex);
    
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "count");
    if (cJSON_IsNumber(item)) app_data.mempool_tx_count = item->valueint;
    
    item = cJSON_GetObjectItemCaseSensitive(json, "vsize");
    if (cJSON_IsNumber(item)) app_data.mempool_size_bytes = item->valueint;
    
    item = cJSON_GetObjectItemCaseSensitive(json, "total_fee");
    if (cJSON_IsNumber(item)) app_data.mempool_total_fee = item->valuedouble;
    
    // Calculate average fee per vbyte
    if (app_data.mempool_size_bytes > 0) {
        app_data.mempool_avg_fee = (app_data.mempool_total_fee * 100000000) / app_data.mempool_size_bytes;
    } else {
        app_data.mempool_avg_fee = 0;
    }
    
    pthread_mutex_unlock(&app_data.data_mutex);
    
    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    g_string_free(response, TRUE);
    
    return TRUE;
}

// Show desktop notification
void show_notification(const gchar *title, const gchar *message, const gchar *icon) {
    if (!notify_is_initted() && !notify_init("Bitcoin Fee Tracker")) {
        g_warning("Failed to initialize notifications");
        return;
    }
    
    NotifyNotification *notification = notify_notification_new(title, message, icon);
    notify_notification_set_timeout(notification, 5000); // 5 seconds
    
    GError *error = NULL;
    if (!notify_notification_show(notification, &error)) {
        g_warning("Failed to show notification: %s", error->message);
        g_error_free(error);
    }
    
    g_object_unref(notification);
}

// Check and trigger alerts
void check_alerts() {
    // Example alert: Notify if fee drops below 10 sat/vB
    if (app_data.fastest_fee < 10.0) {
        char message[256];
        snprintf(message, sizeof(message), "¡La tarifa ha bajado a %.1f sat/vB!", app_data.fastest_fee);
        show_notification("¡Oferta de tarifas bajas!", message, "dialog-information");
    }
    
    // Example alert: Notify if price changes more than 2% in 24h
    if (fabs(app_data.price_change_24h) > 2.0) {
        const char *direction = app_data.price_change_24h > 0 ? "subido" : "bajado";
        char message[256];
        snprintf(message, sizeof(message), "El precio ha %s un %.1f%% en 24h", 
                direction, fabs(app_data.price_change_24h));
        show_notification("Cambio significativo de precio", message, "stock_market-up");
    }
    
    // Example alert: Notify if mempool is congested
    if (app_data.mempool_tx_count > 50000) {
        char message[256];
        snprintf(message, sizeof(message), "¡La mempool está congestionada con %d transacciones!", 
                app_data.mempool_tx_count);
        show_notification("Congestión en la Mempool", message, "dialog-warning");
    }
}

// Callback for the refresh button
static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Unused parameter
    (void)user_data; // Unused parameter
    if (!app_data.is_updating) {
        update_data(NULL);
    }
}

// Initialize application data
static void init_app_data() {
    memset(&app_data, 0, sizeof(AppData));
    pthread_mutex_init(&app_data.data_mutex, NULL);
    app_data.update_interval = 300; // 5 minutes
    app_data.is_updating = FALSE;
    
    // Initialize libnotify
    if (!notify_init("Bitcoin Fee Tracker")) {
        g_warning("Failed to initialize notifications");
    }
    
    // Initialize database
    if (!init_database()) {
        g_warning("Failed to initialize database");
    }
}

// Cleanup function
static void cleanup_app_data() {
    if (app_data.update_timeout_id > 0) {
        g_source_remove(app_data.update_timeout_id);
    }
    
    if (app_data.db) {
        sqlite3_close(app_data.db);
        app_data.db = NULL;
    }
    
    pthread_mutex_destroy(&app_data.data_mutex);
    notify_uninit();
    curl_global_cleanup();
}

// Update the UI with the latest data
static void update_ui(gpointer user_data) {
    (void)user_data; // Unused parameter
    if (!app_data.ui) return;
    
    pthread_mutex_lock(&app_data.data_mutex);
    
    // Update fee information
    ui_update_fee_info(app_data.ui, 
                      app_data.fastest_fee,
                      app_data.half_hour_fee,
                      app_data.hour_fee,
                      app_data.economy_fee,
                      app_data.minimum_fee);
    
    // Update price information
    ui_update_price_info(app_data.ui,
                        app_data.btc_price_usd,
                        app_data.btc_price_eur,
                        app_data.price_change_24h);
    
    // Update mempool information
    ui_update_mempool_info(app_data.ui,
                          app_data.mempool_tx_count,
                          app_data.mempool_size_bytes,
                          app_data.mempool_total_fee,
                          app_data.mempool_avg_fee);
    
    pthread_mutex_unlock(&app_data.data_mutex);
    
    // Check for alerts
    check_alerts();
}

// Thread function to fetch data
static gpointer update_data_thread(gpointer user_data) {
    (void)user_data; // Unused parameter
    
    // Fetch data from APIs
    fetch_fee_data();
    fetch_btc_price();
    fetch_mempool_data();
    
    // Update UI in the main thread
    g_idle_add((GSourceFunc)update_ui, NULL);
    
    app_data.is_updating = FALSE;
    return NULL;
}

// Start a new update
static gboolean update_data(gpointer user_data) {
    if (app_data.is_updating) return FALSE;
    
    app_data.is_updating = TRUE;
    
    // Create a new thread to fetch data
    GThread *thread = g_thread_new("update_thread", update_data_thread, NULL);
    if (!thread) {
        g_warning("Failed to create update thread");
        app_data.is_updating = FALSE;
        return FALSE;
    }
    
    g_thread_unref(thread);
    return FALSE;
}

// Schedule the next update
static void schedule_next_update() {
    if (app_data.update_timeout_id > 0) {
        g_source_remove(app_data.update_timeout_id);
    }
    
    app_data.update_timeout_id = g_timeout_add_seconds(
        app_data.update_interval,
        update_data,
        NULL
    );
}

// Application activate callback
static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data; // Unused parameter
    // Initialize application data
    init_app_data();
    
    // Create the main window and UI
    app_data.ui = ui_init(app);
    if (!app_data.ui) {
        g_error("Failed to initialize UI");
        return;
    }
    
    // Show the window
    gtk_widget_show_all(app_data.ui->window);
    
    // Start the first update
    update_data(NULL);
    
    // Schedule the next update
    schedule_next_update();
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    
    // Create the application
    app = gtk_application_new("com.example.btcfeegui", G_APPLICATION_FLAGS_NONE);
    
    // Connect signals
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(cleanup_app_data), NULL);
    
    // Run the application
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    // Cleanup
    g_object_unref(app);
    
    return status;
}
