#include "ui_utils.h"
#include "chart_utils.h"
#include <string.h>
#include <math.h>
#include <libnotify/notify.h>
#include <stdlib.h>
#include <time.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

// Constantes
#define CONFIG_DIR ".config/gas-fee-tracker"
#define CONFIG_FILE "config.ini"
#define MAX_HISTORY_ENTRIES 1000

// Variables globales
static NotifyNotification *global_notification = NULL;
static const char *currency_symbols[] = {
    [COIN_BTC] = "₿",
    [COIN_ETH] = "Ξ",
    [COIN_LTC] = "Ł"
};

// Estructura para almacenar el historial de tarifas
typedef struct {
    time_t timestamp;
    double fee_rate;
    const char *fee_type;
} FeeHistoryEntry;

// Variables estáticas
static GList *fee_history = NULL;
static pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;

// Prototipos de funciones estáticas
static void add_to_fee_history(time_t timestamp, double fee, const char *fee_type);

// Prototipos de funciones estáticas
static void apply_css(GtkWidget *widget, GtkCssProvider *provider);
static void on_theme_changed(GtkSwitch *widget, gboolean state, gpointer user_data);
static void on_currency_changed(GtkComboBox *widget, gpointer user_data);
static void save_config(const AppUI *ui);
static void load_config(AppUI *ui);
static gboolean check_and_create_config_dir(void);

// Path to the CSS file (relative to the source directory)
#define CSS_FILE "src/style.css"

/**
 * Aplica estilos CSS a un widget y sus hijos
 */
static void apply_css(GtkWidget *widget, GtkCssProvider *provider) {
    if (!widget || !GTK_IS_WIDGET(widget)) return;
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), 
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Aplicar a los hijos
    if (GTK_IS_CONTAINER(widget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList *iter = children; iter != NULL; iter = iter->next) {
            apply_css(GTK_WIDGET(iter->data), provider);
        }
        g_list_free(children);
    }
}

/**
 * Añade una entrada al historial de tarifas
 */
static void add_to_fee_history(time_t timestamp, double fee, const char *fee_type) {
    FeeHistoryEntry *entry = g_new0(FeeHistoryEntry, 1);
    entry->timestamp = timestamp;
    entry->fee_rate = fee;
    entry->fee_type = g_strdup(fee_type);
    
    pthread_mutex_lock(&history_mutex);
    
    // Añadir al principio de la lista
    fee_history = g_list_prepend(fee_history, entry);
    
    // Mantener un límite de entradas
    if (g_list_length(fee_history) > MAX_HISTORY_ENTRIES) {
        GList *last = g_list_last(fee_history);
        fee_history = g_list_remove_link(fee_history, last);
        g_free(((FeeHistoryEntry*)last->data)->fee_type);
        g_free(last->data);
        g_list_free(last);
    }
    
    pthread_mutex_unlock(&history_mutex);
}

/**
 * Crea el directorio de configuración si no existe
 */
static gboolean check_and_create_config_dir(void) {
    gchar *config_path = g_build_path("/", g_get_home_dir(), CONFIG_DIR, NULL);
    
    if (g_mkdir_with_parents(config_path, 0700) == -1) {
        g_warning("No se pudo crear el directorio de configuración: %s", config_path);
        g_free(config_path);
        return FALSE;
    }
    
    g_free(config_path);
    return TRUE;
}

/**
 * Guarda la configuración de la aplicación
 */
static void save_config(const AppUI *ui) {
    if (!ui) return;
    
    if (!check_and_create_config_dir()) {
        g_warning("No se pudo guardar la configuración: error al crear directorio");
        return;
    }
    
    gchar *config_path = g_build_path(
        "/", 
        g_get_home_dir(), 
        CONFIG_DIR, 
        CONFIG_FILE, 
        NULL
    );
    
    GKeyFile *keyfile = g_key_file_new();
    
    // Guardar preferencias
    g_key_file_set_integer(keyfile, "Appearance", "Theme", ui->current_theme);
    g_key_file_set_boolean(keyfile, "Appearance", "DarkMode", 
                         ui->current_theme == THEME_DARK);
    g_key_file_set_integer(keyfile, "General", "Currency", ui->current_currency);
    g_key_file_set_boolean(keyfile, "Notifications", "Enabled", 
                         ui->notifications_enabled);
    g_key_file_set_integer(keyfile, "General", "RefreshInterval", 
                         ui->refresh_interval);
    
    // Guardar en el archivo
    gsize length;
    gchar *data = g_key_file_to_data(keyfile, &length, NULL);
    
    GError *error = NULL;
    if (!g_file_set_contents(config_path, data, -1, &error)) {
        g_warning("Error al guardar la configuración: %s", error->message);
        g_error_free(error);
    }
    
    g_free(data);
    g_key_file_free(keyfile);
    g_free(config_path);
}

/**
 * Carga la configuración de la aplicación
 */
static void load_config(AppUI *ui) {
    if (!ui) return;
    
    gchar *config_path = g_build_path(
        "/", 
        g_get_home_dir(), 
        CONFIG_DIR, 
        CONFIG_FILE, 
        NULL
    );
    
    // Valores por defecto
    ui->current_theme = THEME_SYSTEM;
    ui->current_currency = COIN_BTC;
    ui->notifications_enabled = TRUE;
    ui->refresh_interval = 60; // 1 minuto
    
    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        g_free(config_path);
        return; // Usar valores por defecto si no existe el archivo
    }
    
    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;
    
    if (!g_key_file_load_from_file(keyfile, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("Error al cargar la configuración: %s", error->message);
        g_error_free(error);
        g_key_file_free(keyfile);
        g_free(config_path);
        return;
    }
    
    // Cargar preferencias
    ui->current_theme = g_key_file_get_integer(
        keyfile, 
        "Appearance", 
        "Theme", 
        &error
    );
    
    if (error) {
        // Si no se pudo cargar el tema, intentar con el modo oscuro
        g_clear_error(&error);
        gboolean dark_mode = g_key_file_get_boolean(
            keyfile, 
            "Appearance", 
            "DarkMode", 
            &error
        );
        
        if (!error) {
            ui->current_theme = dark_mode ? THEME_DARK : THEME_LIGHT;
        }
    }
    
    ui->current_currency = g_key_file_get_integer(
        keyfile, 
        "General", 
        "Currency", 
        NULL
    );
    
    ui->notifications_enabled = g_key_file_get_boolean(
        keyfile, 
        "Notifications", 
        "Enabled", 
        NULL
    );
    
    ui->refresh_interval = g_key_file_get_integer(
        keyfile, 
        "General", 
        "RefreshInterval", 
        NULL
    );
    
    g_key_file_free(keyfile);
    g_free(config_path);
}

/**
 * Maneja el cambio de tema
 */
static void on_theme_changed(GtkSwitch *widget, gboolean state, gpointer user_data) {
    AppUI *ui = (AppUI *)user_data;
    if (!ui) return;
    
    ui->current_theme = state ? THEME_DARK : THEME_LIGHT;
    ui_update_theme(ui, ui->current_theme);
    save_config(ui);
}

/**
 * Maneja el cambio de moneda
 */
static void on_currency_changed(GtkComboBox *widget, gpointer user_data) {
    AppUI *ui = (AppUI *)user_data;
    if (!ui) return;
    
    gint active = gtk_combo_box_get_active(widget);
    if (active >= 0 && active < COIN_COUNT) {
        ui->current_currency = (Cryptocurrency)active;
        save_config(ui);
        // TODO: Actualizar la interfaz con los nuevos datos de la moneda
        if (ui->status_label) {
            const gchar *currency_name = "Bitcoin";
            if (ui->current_currency == COIN_ETH) {
                currency_name = "Ethereum";
            } else if (ui->current_currency == COIN_LTC) {
                currency_name = "Litecoin";
            }
            gchar *status = g_strdup_printf("Moneda cambiada a %s", currency_name);
            gtk_label_set_text(GTK_LABEL(ui->status_label), status);
            g_free(status);
        }
    }
}

/**
 * Inicializa la interfaz de usuario
 */
/**
 * Crea un botón con un icono y una etiqueta
 */
static GtkWidget* create_button_with_icon(const gchar *icon_name, const gchar *label_text) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    GtkWidget *label = gtk_label_new(label_text);
    
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_widget_show_all(box);
    
    GtkWidget *button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button), box);
    gtk_widget_show(button);
    
    return button;
}

/**
 * Crea un menú desplegable para la selección de moneda
 */
static GtkWidget* create_currency_selector(AppUI *ui) {
    GtkWidget *combo = gtk_combo_box_text_new();
    
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "btc", "Bitcoin (BTC)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "eth", "Ethereum (ETH)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "ltc", "Litecoin (LTC)");
    
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), ui->current_currency);
    g_signal_connect(combo, "changed", G_CALLBACK(on_currency_changed), ui);
    
    return combo;
}

/**
 * Crea la barra de herramientas principal
 */
static GtkWidget* create_toolbar(AppUI *ui) {
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Gas Fee Tracker");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Monitoreo en tiempo real");
    
    // Botón de actualización
    GtkWidget *refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(refresh_btn, "Actualizar datos");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), refresh_btn);
    
    // Selector de moneda
    GtkWidget *currency_combo = create_currency_selector(ui);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), currency_combo);
    
    // Botón de configuración
    GtkWidget *settings_btn = gtk_button_new_from_icon_name("preferences-system-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(settings_btn, "Configuración");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), settings_btn);
    
    // Conectar señales
    g_signal_connect_swapped(refresh_btn, "clicked", G_CALLBACK(gtk_widget_show_all), ui->window);
    g_signal_connect_swapped(settings_btn, "clicked", G_CALLBACK(gtk_widget_show_all), ui->window);
    
    return header;
}

/**
 * Crea el panel de tarifas
 */
static GtkWidget* create_fee_panel(void) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    
    // Título
    GtkWidget *title = gtk_label_new("Tarifas Actuales");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
#if GTK_CHECK_VERSION(3, 20, 0)
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title-2");
#else
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title-2");
#endif
    gtk_grid_attach(GTK_GRID(grid), title, 0, 0, 3, 1);
    
    // Tarjetas de tarifas
    const gchar *titles[] = {"Rápido", "30 min", "1 hora", "Económico", "Mínimo"};
    
    for (int i = 0; i < 5; i++) {
        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        GtkStyleContext *card_context = gtk_widget_get_style_context(card);
        gtk_style_context_add_class(card_context, "fee-card");
        
        GtkWidget *label = gtk_label_new(titles[i]);
        GtkWidget *value = gtk_label_new("--");
        GtkStyleContext *value_context = gtk_widget_get_style_context(value);
        gtk_style_context_add_class(value_context, "fee-value");
        
        gtk_box_pack_start(GTK_BOX(card), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card), value, FALSE, FALSE, 0);
        
        gtk_grid_attach(GTK_GRID(grid), card, i % 3, 1 + (i / 3), 1, 1);
    }
    
    return grid;
}

/**
 * Inicializa la interfaz de usuario
 */
AppUI* ui_init(GtkApplication *app) {
    // Inicializar notificaciones
    notify_init("Gas Fee Tracker");
    
    // Crear estructura de la interfaz
    AppUI *ui = g_malloc0(sizeof(AppUI));
    if (!ui) {
        g_critical("No se pudo asignar memoria para la interfaz de usuario");
        return NULL;
    }
    
    // Cargar configuración
    load_config(ui);
    
    // Crear ventana principal
    ui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Gas Fee Tracker");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 1000, 700);
    gtk_window_set_position(GTK_WINDOW(ui->window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(ui->window), 0);
    
    // Configurar la ventana
    gtk_window_set_icon_name(GTK_WINDOW(ui->window), "network-transmit-receive");
    
    // Crear contenedor principal
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), main_box);
    
    // Añadir barra de herramientas
    GtkWidget *header = create_toolbar(ui);
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);
    
    // Crear pila para los paneles
    ui->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(ui->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(ui->stack), 300);
    
    // Crear contenedor para el panel principal
    GtkWidget *dashboard_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(dashboard_box), 10);
    
    // Crear y configurar gráficos
    // Gráfico de tarifas
    GtkWidget *fee_chart_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *fee_chart_label = gtk_label_new("Historial de Tarifas");
    gtk_widget_set_halign(fee_chart_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(fee_chart_container), fee_chart_label, FALSE, FALSE, 0);
    
    // Inicializar gráfico de tarifas
    ui->fee_chart = chart_config_new(NULL, "Evolución de Tarifas");
    if (ui->fee_chart) {
        // Configurar colores de las series
        GdkRGBA color_fast = {0.8, 0.2, 0.2, 1.0};  // Rojo
        GdkRGBA color_avg = {0.2, 0.6, 0.2, 1.0};   // Verde
        GdkRGBA color_slow = {0.2, 0.2, 0.8, 1.0};  // Azul
        GdkRGBA color_eco = {0.8, 0.5, 0.2, 1.0};   // Naranja
        
        // Añadir series al gráfico
        chart_add_series(ui->fee_chart, "Rápido", &color_fast, TRUE);
        chart_add_series(ui->fee_chart, "Media Hora", &color_avg, TRUE);
        chart_add_series(ui->fee_chart, "1 Hora", &color_slow, TRUE);
        chart_add_series(ui->fee_chart, "Económico", &color_eco, TRUE);
        
        // Añadir el gráfico al contenedor
        gtk_box_pack_start(GTK_BOX(fee_chart_container), ui->fee_chart->drawing_area, TRUE, TRUE, 0);
    }
    
    // Gráfico de precios
    GtkWidget *price_chart_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *price_chart_label = gtk_label_new("Historial de Precios");
    gtk_widget_set_halign(price_chart_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(price_chart_container), price_chart_label, FALSE, FALSE, 0);
    
    // Inicializar gráfico de precios
    ui->price_chart = chart_config_new(NULL, "Precio de Bitcoin");
    if (ui->price_chart) {
        GdkRGBA color_price = {0.6, 0.2, 0.6, 1.0};  // Púrpura
        chart_add_series(ui->price_chart, "Precio USD", &color_price, TRUE);
        gtk_box_pack_start(GTK_BOX(price_chart_container), ui->price_chart->drawing_area, TRUE, TRUE, 0);
    }
    
    // Gráfico de mempool
    GtkWidget *mempool_chart_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *mempool_chart_label = gtk_label_new("Estadísticas de Mempool");
    gtk_widget_set_halign(mempool_chart_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(mempool_chart_container), mempool_chart_label, FALSE, FALSE, 0);
    
    // Inicializar gráfico de mempool
    ui->mempool_chart = chart_config_new(NULL, "Estadísticas de Mempool");
    if (ui->mempool_chart) {
        GdkRGBA color_tx = {0.2, 0.6, 0.8, 1.0};    // Azul claro
        GdkRGBA color_size = {0.8, 0.6, 0.2, 1.0};  // Amarillo
        GdkRGBA color_fee = {0.8, 0.2, 0.2, 1.0};   // Rojo
        
        chart_add_series(ui->mempool_chart, "Transacciones", &color_tx, TRUE);
        chart_add_series(ui->mempool_chart, "Tamaño (MB)", &color_size, TRUE);
        chart_add_series(ui->mempool_chart, "Tarifa Media", &color_fee, TRUE);
        
        gtk_box_pack_start(GTK_BOX(mempool_chart_container), ui->mempool_chart->drawing_area, TRUE, TRUE, 0);
    }
    
    // Organizar los gráficos en el panel
    GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(dashboard_box), top_box, TRUE, TRUE, 0);
    
    // Panel izquierdo (tarifas y precios)
    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(top_box), left_panel, TRUE, TRUE, 0);
    
    // Panel de tarifas
    GtkWidget *fee_panel = create_fee_panel();
    gtk_box_pack_start(GTK_BOX(left_panel), fee_panel, FALSE, FALSE, 0);
    
    // Gráfico de tarifas
    gtk_box_pack_start(GTK_BOX(left_panel), fee_chart_container, TRUE, TRUE, 0);
    
    // Panel derecho (precios y mempool)
    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(top_box), right_panel, TRUE, TRUE, 0);
    
    // Gráfico de precios
    gtk_box_pack_start(GTK_BOX(right_panel), price_chart_container, TRUE, TRUE, 0);
    
    // Gráfico de mempool
    gtk_box_pack_start(GTK_BOX(right_panel), mempool_chart_container, TRUE, TRUE, 0);
    
    // Añadir el panel principal a la pila
    gtk_stack_add_titled(GTK_STACK(ui->stack), dashboard_box, "dashboard", "Panel Principal");
    
    // Añadir pila al contenedor principal
    gtk_box_pack_start(GTK_BOX(main_box), ui->stack, TRUE, TRUE, 0);
    
    // Añadir barra de estado
    ui->status_bar = gtk_statusbar_new();
    gtk_box_pack_end(GTK_BOX(main_box), ui->status_bar, FALSE, FALSE, 0);
    
    // Inicializar etiquetas de estado
    ui->status_label = gtk_label_new("Listo");
    gtk_widget_set_halign(ui->status_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(ui->status_label, 10);
    gtk_box_pack_start(GTK_BOX(ui->status_bar), ui->status_label, FALSE, FALSE, 0);
    
    // Configuración de la interfaz de usuario
    
    // Configurar el tema oscuro
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, NULL);
    
    // Cargar estilos CSS desde archivo
    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;
    
    gchar *css_path = g_build_filename(g_get_current_dir(), CSS_FILE, NULL);
    
    if (!gtk_css_provider_load_from_file(provider, g_file_new_for_path(css_path), &error)) {
        g_warning("Error loading CSS file '%s': %s", css_path, error->message);
        g_error_free(error);
    } else {
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }
    
    g_free(css_path);
    // Crear el layout principal
    ui->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), ui->main_box);
    
    // Crear el encabezado
    ui->header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->header_box, FALSE, FALSE, 0);
    gtk_widget_set_name(ui->header_box, "header");
    
    // Título de la aplicación
    GtkWidget *title_label = gtk_label_new("BITCOIN FEE TRACKER PRO");
    gtk_widget_set_name(title_label, "header-title");
    gtk_box_pack_start(GTK_BOX(ui->header_box), title_label, FALSE, FALSE, 10);
    
    // Espaciador
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(ui->header_box), spacer, TRUE, TRUE, 0);
    
    // Botón de actualización manual
    ui->refresh_button = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(ui->refresh_button, "Actualizar datos");
    gtk_box_pack_end(GTK_BOX(ui->header_box), ui->refresh_button, FALSE, FALSE, 5);
    
    // Contenedor principal
    ui->content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->content_box, TRUE, TRUE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->content_box), 10);
    
    // Crear el notebook (pestañas)
    ui->notebook = gtk_notebook_new();
    gtk_widget_set_name(ui->notebook, "notebook");
    gtk_box_pack_start(GTK_BOX(ui->content_box), ui->notebook, TRUE, TRUE, 0);
    
    // Pestaña de Resumen
    GtkWidget *summary_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(summary_box), 10);
    
    // Fila de tarjetas de tarifas
    GtkWidget *fee_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(summary_box), fee_box, FALSE, FALSE, 0);
    
    // Tarjeta de tarifa más rápida
    GtkWidget *fastest_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_name(fastest_card, "card");
    gtk_widget_set_size_request(fastest_card, 160, 120);
    gtk_box_pack_start(GTK_BOX(fee_box), fastest_card, TRUE, TRUE, 0);
    
    GtkWidget *fastest_label = gtk_label_new("MÁS RÁPIDO");
    gtk_widget_set_name(fastest_label, "card-title");
    gtk_box_pack_start(GTK_BOX(fastest_card), fastest_label, FALSE, FALSE, 0);
    
    ui->fee_label = gtk_label_new("--");
    gtk_widget_set_name(ui->fee_label, "fee-value");
    gtk_box_pack_start(GTK_BOX(fastest_card), ui->fee_label, TRUE, TRUE, 0);
    
    GtkWidget *fastest_desc = gtk_label_new("sat/vB");
    gtk_widget_set_name(fastest_desc, "fee-label");
    gtk_box_pack_start(GTK_BOX(fastest_card), fastest_desc, FALSE, FALSE, 0);
    
    // (Añadir más tarjetas de tarifas aquí...)
    
    // Fila de precios y mempool
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(summary_box), info_box, TRUE, TRUE, 0);
    
    // Tarjeta de precio
    GtkWidget *price_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_name(price_card, "card");
    gtk_box_pack_start(GTK_BOX(info_box), price_card, TRUE, TRUE, 0);
    
    GtkWidget *price_title = gtk_label_new("PRECIO BTC");
    gtk_widget_set_name(price_title, "card-title");
    gtk_box_pack_start(GTK_BOX(price_card), price_title, FALSE, FALSE, 0);
    
    ui->price_label = gtk_label_new("Cargando...");
    gtk_widget_set_name(ui->price_label, "fee-value");
    gtk_box_pack_start(GTK_BOX(price_card), ui->price_label, TRUE, TRUE, 0);
    
    // Tarjeta de mempool
    GtkWidget *mempool_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_name(mempool_card, "card");
    gtk_box_pack_start(GTK_BOX(info_box), mempool_card, TRUE, TRUE, 0);
    
    GtkWidget *mempool_title = gtk_label_new("MEMPOOL");
    gtk_widget_set_name(mempool_title, "card-title");
    gtk_box_pack_start(GTK_BOX(mempool_card), mempool_title, FALSE, FALSE, 0);
    
    ui->mempool_label = gtk_label_new("Transacciones: --\nTamaño: -- MB\nTarifa media: -- sat/vB");
    gtk_label_set_line_wrap(GTK_LABEL(ui->mempool_label), TRUE);
    gtk_widget_set_halign(ui->mempool_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(mempool_card), ui->mempool_label, TRUE, TRUE, 0);
    
    // Añadir pestaña de resumen
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), summary_box, gtk_label_new("Resumen"));
    
    // Pestaña de gráficos
    ui->charts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->charts_box), 10);
    
    // Inicializar gráficos
    ui->fee_chart = chart_config_new(ui->charts_box, "Historial de Tarifas (sat/vB)");
    chart_add_series(ui->fee_chart, "Tarifa más rápida", NULL, TRUE);
    
    ui->price_chart = chart_config_new(ui->charts_box, "Precio de Bitcoin (USD)");
    chart_add_series(ui->price_chart, "Precio USD", NULL, TRUE);
    
    ui->mempool_chart = chart_config_new(ui->charts_box, "Tamaño de la Mempool (MB)");
    chart_add_series(ui->mempool_chart, "Tamaño MB", NULL, FALSE);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), ui->charts_box, gtk_label_new("Gráficos"));
    
    // Pestaña de alertas
    ui->alerts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->alerts_box), 10);
    
    // Crear modelo para la lista de alertas
    ui->alerts_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_STRING);
    GtkWidget *alerts_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ui->alerts_store));
    ui->alerts_view = alerts_view;
    
    // Configurar columnas
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Columna de tipo
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Tipo", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(alerts_view), column);
    
    // Columna de condición
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Condición", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(alerts_view), column);
    
    // Columna de valor
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Valor", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(alerts_view), column);
    
    // Columna de estado
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Estado", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(alerts_view), column);
    
    // Añadir la vista de alertas al contenedor
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled), alerts_view);
    gtk_box_pack_start(GTK_BOX(ui->alerts_box), scrolled, TRUE, TRUE, 0);
    
    // Añadir pestaña de alertas
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), ui->alerts_box, gtk_label_new("Alertas"));
    
    // Añadir pestaña de configuración
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), ui->config_box, gtk_label_new("Configuración"));
    
    // Inicializar la barra de estado
    ui->footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(ui->footer_box, "status-bar");
    gtk_box_pack_end(GTK_BOX(ui->main_box), ui->footer_box, FALSE, FALSE, 0);
    
    return ui;
}

// ...

// Liberar recursos de la aplicación
void ui_cleanup(AppUI *ui) {
    if (!ui) return;

    // Liberar recursos de notificaciones
    notify_uninit();

    // Liberar recursos de los gráficos
    if (ui->fee_chart) {
        chart_config_free(ui->fee_chart);
        ui->fee_chart = NULL;
    }

    if (ui->price_chart) {
        chart_config_free(ui->price_chart);
        ui->price_chart = NULL;
    }

    if (ui->mempool_chart) {
        chart_config_free(ui->mempool_chart);
        ui->mempool_chart = NULL;
    }

    // Liberar proveedor CSS
    if (ui->css_provider) {
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(ui->css_provider)
        );
        g_object_unref(ui->css_provider);
    }

    // Liberar ventana principal
    if (ui->window) {
        gtk_widget_destroy(ui->window);
    }

    g_free(ui);
}

void ui_update_fee_info(
    AppUI *ui, 
    double fastest, 
    double halfHour, 
    double hour, 
    double economy, 
    double minimum
) {
    if (ui && ui->fee_label) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), 
                "<b>Tarifas actuales:</b> Rápido: %.0f sat/vB | 30 min: %.0f sat/vB | 1h: %.0f sat/vB | Económico: %.0f sat/vB | Mínimo: %.0f sat/vB",
                fastest, halfHour, hour, economy, minimum);
        gtk_label_set_markup(GTK_LABEL(ui->fee_label), buffer);
    }
    
    // Actualizar etiquetas individuales si están inicializadas
    if (ui && ui->fee_labels[0]) {
        char buffer[32];
        
        snprintf(buffer, sizeof(buffer), "%.0f sat/vB", fastest);
        gtk_label_set_text(GTK_LABEL(ui->fee_labels[0]), buffer);
        
        snprintf(buffer, sizeof(buffer), "%.0f sat/vB", halfHour);
        gtk_label_set_text(GTK_LABEL(ui->fee_labels[1]), buffer);
        
        snprintf(buffer, sizeof(buffer), "%.0f sat/vB", hour);
        gtk_label_set_text(GTK_LABEL(ui->fee_labels[2]), buffer);
        
        snprintf(buffer, sizeof(buffer), "%.0f sat/vB", economy);
        gtk_label_set_text(GTK_LABEL(ui->fee_labels[3]), buffer);
        
        snprintf(buffer, sizeof(buffer), "%.0f sat/vB", minimum);
        gtk_label_set_text(GTK_LABEL(ui->fee_labels[4]), buffer);
    }
    
    // Actualizar gráfico de tarifas
    if (ui->fee_chart) {
        time_t now = time(NULL);
        
        // Añadir puntos a las series del gráfico
        chart_add_point(ui->fee_chart, "Rápido", now, fastest);
        chart_add_point(ui->fee_chart, "Media Hora", now, halfHour);
        chart_add_point(ui->fee_chart, "1 Hora", now, hour);
        chart_add_point(ui->fee_chart, "Económico", now, economy);
        
        // Redibujar el gráfico
        gtk_widget_queue_draw(ui->fee_chart->drawing_area);
    }
    
    // Actualizar estado
    char status[256];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(status, sizeof(status), "Actualizado: %H:%M:%S", tm_info);
    gtk_label_set_text(GTK_LABEL(ui->status_label), status);
}

// Actualiza la información de precios en la interfaz
void ui_update_price_info(AppUI *ui, double usd, double eur, double change24h) {
    // Actualizar etiquetas de precios
    if (ui->price_label) {
        char buffer[128];
        const char *trend = (change24h >= 0) ? "▲" : "▼";
        const char *color = (change24h >= 0) ? "#4CAF50" : "#F44336";
        
        snprintf(buffer, sizeof(buffer), 
                "<span size='x-large'><b>$%.2f</b></span>  "
                "<span size='small' color='gray'>(€%.2f)</span>  "
                "<span size='small' color='%s'>%s %.2f%%</span>",
                usd, eur, color, trend, fabs(change24h));
                
        gtk_label_set_markup(GTK_LABEL(ui->price_label), buffer);
    }
    
    // Actualizar gráfico de precios
    if (ui->price_chart) {
        time_t now = time(NULL);
        chart_add_point(ui->price_chart, "Precio USD", now, usd);
        gtk_widget_queue_draw(ui->price_chart->drawing_area);
    }
}

// Actualiza la información de la mempool en la interfaz
void ui_update_mempool_info(AppUI *ui, int count, int size, double total_fee, double avg_fee) {
    // Actualizar etiqueta de mempool
    if (ui->mempool_label) {
        char buffer[256];
        double size_mb = size / 1024.0 / 1024.0; // Convertir a MB
        
        snprintf(buffer, sizeof(buffer),
                "<b>Mempool:</b> %d transacciones (%.2f MB)  |  "
                "<b>Tarifa media:</b> %.2f sat/vB  |  "
                "<b>Tarifa total:</b> %.4f BTC",
                count, size_mb, avg_fee, total_fee);
                
        gtk_label_set_markup(GTK_LABEL(ui->mempool_label), buffer);
    }
    
    // Actualizar gráfico de mempool
    if (ui->mempool_chart) {
        time_t now = time(NULL);
        
        // Añadir puntos a las series del gráfico
        chart_add_point(ui->mempool_chart, "Transacciones", now, count);
        chart_add_point(ui->mempool_chart, "Tamaño (MB)", now, size / 1024.0 / 1024.0);
        chart_add_point(ui->mempool_chart, "Tarifa Media", now, avg_fee);
        
        // Redibujar el gráfico
        gtk_widget_queue_draw(ui->mempool_chart->drawing_area);
    }
    // Chart updates complete
}

// Añade una alerta a la lista
void ui_add_alert(AppUI *ui, const char *type, const char *condition, double value, const char *status) {
    if (!ui || !ui->alerts_store) return;
    
    GtkTreeIter iter;
    gtk_list_store_append(ui->alerts_store, &iter);
    gtk_list_store_set(ui->alerts_store, &iter,
                      0, type,
                      1, condition,
                      2, value,
                      3, status,
                      -1);
}
