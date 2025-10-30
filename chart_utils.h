#ifndef CHART_UTILS_H
#define CHART_UTILS_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <time.h>

// Structure for a single data point
typedef struct {
    double x;  // x-coordinate (usually time)
    double y;  // y-coordinate (value)
} ChartDataPoint;

// Structure for a chart series
typedef struct {
    char *label;           // Series label
    GdkRGBA color;         // Series color
    GArray *data;          // Array of ChartDataPoint
    gboolean show_points;  // Whether to show data points
    gboolean visible;      // Whether the series is visible
} ChartSeries;

// Chart configuration structure
typedef struct {
    GtkWidget *drawing_area;  // Drawing area widget
    GList *series;            // List of ChartSeries
    char *title;              // Chart title
    GdkRGBA bg_color;         // Background color
    GdkRGBA grid_color;       // Grid color
    GdkRGBA text_color;       // Text color
    double min_x, max_x;      // X-axis range
    double min_y, max_y;      // Y-axis range
    double zoom_level;        // Current zoom level
    double pan_offset;        // Current pan offset
} ChartConfig;

// Public functions
ChartConfig* chart_config_new(GtkWidget *parent, const char *title);
void chart_config_free(ChartConfig *config);
void chart_add_series(ChartConfig *config, const char *label, const GdkRGBA *color, 
                     gboolean show_points);
void chart_add_data(ChartConfig *config, int series_index, double value);
void chart_add_point(ChartConfig *chart, const char *series_name, time_t timestamp, double value);
void chart_clear_series(ChartConfig *config, int series_index);
void chart_redraw(ChartConfig *config);
void chart_set_time_range(ChartConfig *config, int64_t start, int64_t end);
void chart_reset_zoom(ChartConfig *config);

// Drawing functions
static gboolean chart_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static void chart_draw_grid(ChartConfig *config, cairo_t *cr, int width, int height);
static void chart_draw_series(ChartConfig *config, cairo_t *cr, int width, int height);
static void chart_draw_legend(ChartConfig *config, cairo_t *cr, int width, int height);

// Event handlers
static gboolean chart_motion_notify_cb(GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
static gboolean chart_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean chart_button_release_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean chart_scroll_cb(GtkWidget *widget, GdkEventScroll *event, gpointer user_data);

// Chart types
typedef enum {
    CHART_TYPE_LINE,     // Line chart
    CHART_TYPE_AREA,     // Area chart
    CHART_TYPE_BAR,      // Bar chart
    CHART_TYPE_CANDLE    // Velas japonesas
} ChartType;

// Configuración de velas japonesas
typedef struct {
    double open;
    double high;
    double low;
    double close;
    time_t timestamp;
} CandleData;

// Configuración avanzada de gráficos
typedef struct {
    ChartType type;           // Tipo de gráfico
    gboolean show_volume;     // Mostrar volumen
    gboolean log_scale;       // Escala logarítmica
    gboolean auto_scale;      // Autoescalado
    int64_t time_window;      // Ventana de tiempo en segundos
    int update_interval;      // Intervalo de actualización
} ChartAdvancedConfig;

// Funciones avanzadas
void chart_set_type(ChartConfig *config, ChartType type);
void chart_set_advanced_config(ChartConfig *config, const ChartAdvancedConfig *adv_config);
void chart_add_candle_data(ChartConfig *config, const CandleData *candles, int count);
void chart_enable_zoom(ChartConfig *config, gboolean enable);
void chart_enable_pan(ChartConfig *config, gboolean enable);
void chart_export_to_png(ChartConfig *config, const char *filename);
void chart_export_to_svg(ChartConfig *config, const char *filename);

#endif // CHART_UTILS_H
