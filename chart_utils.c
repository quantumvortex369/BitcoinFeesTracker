#include "chart_utils.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <pango/pangocairo.h>

// Default colors
static const GdkRGBA DEFAULT_COLORS[] = {
    {0.95, 0.26, 0.21, 1.0},  // Red
    {0.96, 0.80, 0.09, 1.0},  // Yellow
    {0.16, 0.63, 0.60, 1.0},  // Teal
    {0.40, 0.65, 0.99, 1.0},  // Blue
    {0.74, 0.18, 0.95, 1.0},  // Purple
    {1.00, 0.44, 0.37, 1.0},  // Orange
    {0.30, 0.69, 0.31, 1.0}   // Green
};

// Default padding
#define CHART_PADDING 10

// Get a color from the default palette
static void get_default_color(int index, GdkRGBA *color) {
    *color = DEFAULT_COLORS[index % (sizeof(DEFAULT_COLORS) / sizeof(DEFAULT_COLORS[0]))];
}

// Create a new chart configuration
ChartConfig* chart_config_new(GtkWidget *parent, const char *title) {
    ChartConfig *config = g_new0(ChartConfig, 1);
    
    // Create drawing area
    config->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(config->drawing_area, 600, 300);
    gtk_widget_add_events(config->drawing_area, 
                         GDK_POINTER_MOTION_MASK | 
                         GDK_BUTTON_PRESS_MASK | 
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_SCROLL_MASK);
    
    // Set default colors
    gdk_rgba_parse(&config->bg_color, "#1E1E2E");
    gdk_rgba_parse(&config->grid_color, "#45475A");
    gdk_rgba_parse(&config->text_color, "#CDD6F4");
    
    // Initialize series list
    config->series = NULL;
    
    // Set default ranges
    config->min_x = 0;
    config->max_x = 1;
    config->min_y = 0;
    config->max_y = 1;
    config->zoom_level = 1.0;
    config->pan_offset = 0.0;
    
    // Set title if provided
    if (title) {
        config->title = g_strdup(title);
    }
    
    // Connect signals
    g_signal_connect(config->drawing_area, "draw", 
                    G_CALLBACK(chart_draw_cb), config);
    g_signal_connect(config->drawing_area, "motion-notify-event", 
                    G_CALLBACK(chart_motion_notify_cb), config);
    g_signal_connect(config->drawing_area, "button-press-event",
                    G_CALLBACK(chart_button_press_cb), config);
    g_signal_connect(config->drawing_area, "button-release-event",
                    G_CALLBACK(chart_button_release_cb), config);
    g_signal_connect(config->drawing_area, "scroll-event",
                    G_CALLBACK(chart_scroll_cb), config);
    
    // Add to parent if provided
    if (parent) {
        gtk_container_add(GTK_CONTAINER(parent), config->drawing_area);
    }
    
    return config;
}

// Add a new data point to a chart series
void chart_add_point(ChartConfig *chart, const char *series_name, time_t timestamp, double value) {
    if (!chart || !series_name) return;
    
    // Find the series
    ChartSeries *series = NULL;
    GList *iter = chart->series;
    while (iter) {
        ChartSeries *s = (ChartSeries *)iter->data;
        if (s && strcmp(s->label, series_name) == 0) {
            series = s;
            break;
        }
        iter = g_list_next(iter);
    }
    
    // If series not found, create it
    if (!series) {
        GdkRGBA color;
        get_default_color(g_list_length(chart->series), &color);
        chart_add_series(chart, series_name, &color, TRUE);
        series = (ChartSeries *)g_list_last(chart->series)->data;
    }
    
    // Add the data point
    if (series->data == NULL) {
        series->data = g_array_new(FALSE, FALSE, sizeof(ChartDataPoint));
    }
    
    ChartDataPoint point = { (double)timestamp, value };
    g_array_append_val(series->data, point);
    
    // Update chart bounds if needed
    if (value < chart->min_y) chart->min_y = value * 0.95;
    if (value > chart->max_y) chart->max_y = value * 1.05;
    if (chart->min_y == chart->max_y) {
        chart->min_y *= 0.9;
        chart->max_y *= 1.1;
    }
    
    // Queue redraw
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }
}

// Clean up chart resources
void chart_config_free(ChartConfig *config) {
    if (!config) return;
    
    // Free series data
    GList *iter = config->series;
    while (iter) {
        ChartSeries *series = (ChartSeries *)iter->data;
        if (series) {
            g_free(series->label);
            if (series->data) {
                g_array_free(series->data, TRUE);
            }
            g_free(series);
        }
        iter = g_list_next(iter);
    }
    g_list_free(config->series);
    
    // Free title
    g_free(config->title);
    
    // Free config
    g_free(config);
}

// Add a new series to the chart
void chart_add_series(ChartConfig *config, const char *label, const GdkRGBA *color, 
                     gboolean show_points) {
    if (!config || !label) return;
    
    // Create new series
    ChartSeries *series = g_new0(ChartSeries, 1);
    series->label = g_strdup(label);
    if (color) {
        series->color = *color;
    } else {
        // Default color if none provided
        GdkRGBA default_color = {0.0, 0.0, 1.0, 1.0}; // Blue
        get_default_color(g_list_length(config->series), &default_color);
        series->color = default_color;
    }
    series->show_points = show_points;
    series->visible = TRUE;
    series->data = g_array_new(FALSE, FALSE, sizeof(ChartDataPoint));
    
    // Add to series list
    config->series = g_list_append(config->series, series);
    
    // Queue redraw
    if (config->drawing_area) {
        gtk_widget_queue_draw(config->drawing_area);
    }
}

// Add a data point to a series
void chart_add_data(ChartConfig *config, int series_index, double value) {
    if (!config) return;
    
    // Get the series
    GList *item = g_list_nth(config->series, series_index);
    if (!item) return;
    
    ChartSeries *series = (ChartSeries *)item->data;
    if (!series) return;
    
    // Add data point with current time
    time_t now = time(NULL);
    chart_add_point(config, series->label, now, value);
}

// Clear a data series
void chart_clear_series(ChartConfig *config, int series_index) {
    if (!config) return;
    
    // Get the series
    GList *item = g_list_nth(config->series, series_index);
    if (!item) return;
    
    ChartSeries *series = (ChartSeries *)item->data;
    if (!series) return;
    
    // Clear data
    if (series->data) {
        g_array_remove_range(series->data, 0, series->data->len);
    }
    
    // Queue redraw
    if (config->drawing_area) {
        gtk_widget_queue_draw(config->drawing_area);
    }
}

// Set the time range for the chart
void chart_set_time_range(ChartConfig *config, int64_t start, int64_t end) {
    if (!config) return;
    
    config->min_x = start;
    config->max_x = end;
    
    // Queue redraw
    if (config->drawing_area) {
        gtk_widget_queue_draw(config->drawing_area);
    }
}

// Draw grid lines
static void chart_draw_grid(ChartConfig *config, cairo_t *cr, int width, int height) {
    if (!config || !cr) return;
    
    // Draw background
    gdk_cairo_set_source_rgba(cr, &config->bg_color);
    cairo_paint(cr);
    
    // Set grid style
    cairo_set_line_width(cr, 0.5);
    gdk_cairo_set_source_rgba(cr, &config->grid_color);
    
    // Draw vertical grid lines (time axis)
    int num_x_ticks = 5;
    for (int i = 0; i <= num_x_ticks; i++) {
        double x = (width * i) / num_x_ticks;
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
        cairo_stroke(cr);
    }
    
    // Draw horizontal grid lines (value axis)
    int num_y_ticks = 5;
    for (int i = 0; i <= num_y_ticks; i++) {
        double y = (height * i) / num_y_ticks;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
        cairo_stroke(cr);
    }
}

// Draw a single data series
static void chart_draw_series(ChartConfig *config, cairo_t *cr, int width, int height) {
    if (!config || !config->series) return;
    
    GList *iter = config->series;
    while (iter) {
        ChartSeries *series = (ChartSeries *)iter->data;
        if (series && series->visible && series->data && series->data->len > 0) {
            // Set line style
            gdk_cairo_set_source_rgba(cr, &series->color);
            cairo_set_line_width(cr, 2.0);
            
            // Draw the line
            gboolean first = TRUE;
            for (guint i = 0; i < series->data->len; i++) {
                ChartDataPoint *point = &g_array_index(series->data, ChartDataPoint, i);
                double x = (point->x - config->min_x) * width / (config->max_x - config->min_x);
                double y = height - (point->y - config->min_y) * height / (config->max_y - config->min_y);
                
                if (first) {
                    cairo_move_to(cr, x, y);
                    first = FALSE;
                } else {
                    cairo_line_to(cr, x, y);
                }
            }
            cairo_stroke(cr);
            
            // Draw points if enabled
            if (series->show_points) {
                for (guint i = 0; i < series->data->len; i++) {
                    ChartDataPoint *point = &g_array_index(series->data, ChartDataPoint, i);
                    double x = (point->x - config->min_x) * width / (config->max_x - config->min_x);
                    double y = height - (point->y - config->min_y) * height / (config->max_y - config->min_y);
                    
                    cairo_arc(cr, x, y, 3.0, 0, 2 * G_PI);
                    cairo_fill(cr);
                }
            }
            for (guint i = 0; i < series->data->len; i++) {
                ChartDataPoint *point = &g_array_index(series->data, ChartDataPoint, i);
                
                // Map data coordinates to screen coordinates
                double x = ((point->x - config->min_x) / (config->max_x - config->min_x)) * width;
                double y = height - ((point->y - config->min_y) / (config->max_y - config->min_y)) * height;
                
                if (first) {
                    cairo_move_to(cr, x, y);
                    first = FALSE;
                } else {
                    cairo_line_to(cr, x, y);
                }
                
                // Draw point if enabled
                if (series->show_points) {
                    cairo_arc(cr, x, y, 2.5, 0, 2 * G_PI);
                    cairo_fill(cr);
                }
            }
            
            // Stroke the line
            cairo_stroke(cr);
        }
        iter = g_list_next(iter);
    }
}

// Draw the chart legend
static void chart_draw_legend(ChartConfig *config, cairo_t *cr, int width, int height) {
    if (!config || !config->series) return;
    
    const int legend_padding = 10;
    const int legend_item_height = 20;
    const int legend_swatch_size = 12;
    const int legend_text_padding = 5;
    
    // Count visible series
    int visible_count = 0;
    GList *iter = config->series;
    while (iter) {
        ChartSeries *series = (ChartSeries *)iter->data;
        if (series && series->visible) visible_count++;
        iter = g_list_next(iter);
    }
    
    if (visible_count == 0) return;
    
    // Calculate legend dimensions
    int legend_width = 150;
    int legend_height = visible_count * legend_item_height + 2 * legend_padding;
    int legend_x = width - legend_width - legend_padding;
    int legend_y = legend_padding;
    
    // Draw legend background
    GdkRGBA bg = {0.1, 0.1, 0.1, 0.8};
    gdk_cairo_set_source_rgba(cr, &bg);
    cairo_rectangle(cr, legend_x, legend_y, legend_width, legend_height);
    cairo_fill(cr);
    
    // Draw legend items
    int item_y = legend_y + legend_padding;
    iter = config->series;
    while (iter) {
        ChartSeries *series = (ChartSeries *)iter->data;
        if (series && series->visible) {
            // Draw color swatch
            gdk_cairo_set_source_rgba(cr, &series->color);
            cairo_rectangle(cr, legend_x + legend_padding, item_y, 
                          legend_swatch_size, legend_swatch_size);
            cairo_fill(cr);
            
            // Draw label
            gdk_cairo_set_source_rgba(cr, &config->text_color);
            PangoLayout *layout = gtk_widget_create_pango_layout(config->drawing_area, series->label);
            cairo_move_to(cr, legend_x + legend_padding + legend_swatch_size + legend_text_padding, 
                         item_y - 3);
            pango_cairo_show_layout(cr, layout);
            g_object_unref(layout);
            
            item_y += legend_item_height;
        }
        iter = g_list_next(iter);
    }
}


// Main drawing callback
static gboolean chart_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    ChartConfig *config = (ChartConfig *)user_data;
    if (!config) return FALSE;
    
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int width = allocation.width;
    int height = allocation.height;
    
    // Set background color
    gdk_cairo_set_source_rgba(cr, &config->bg_color);
    cairo_paint(cr);
    
    // Draw grid
    chart_draw_grid(config, cr, width, height);
    
    // Draw series
    chart_draw_series(config, cr, width, height);
    
    // Draw legend
    chart_draw_legend(config, cr, width, height);
    
    return FALSE;
}

// Mouse motion event handler
static gboolean chart_motion_notify_cb(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    ChartConfig *config = (ChartConfig*)user_data;
    if (!config) return FALSE;
    
    // Handle panning if left button is pressed
    if (event->state & GDK_BUTTON1_MASK) {
        static gdouble last_x = 0;
        
        if (last_x > 0) {
            double dx = event->x - last_x;
            double range = config->max_x - config->min_x;
            double visible_range = range / config->zoom_level;
            
            // Update pan offset
            config->pan_offset += dx * (visible_range / gtk_widget_get_allocated_width(widget));
            
            // Clamp pan offset to valid range
            double max_pan = range * (1.0 - 1.0/config->zoom_level);
            if (config->pan_offset > max_pan) config->pan_offset = max_pan;
            if (config->pan_offset < 0) config->pan_offset = 0;
            
            // Queue redraw
            gtk_widget_queue_draw(widget);
        }
        
        last_x = event->x;
    }
    
    return TRUE;
}

// Button press event handler
static gboolean chart_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    // Can be implemented for handling button press events
    return FALSE;
}

// Button release event handler
static gboolean chart_button_release_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    // Can be implemented for handling button release events
    return FALSE;
}

// Scroll event handler for zooming
static gboolean chart_scroll_cb(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
    ChartConfig *config = (ChartConfig *)user_data;
    if (!config) return FALSE;
    
    double zoom_factor = (event->direction == GDK_SCROLL_UP) ? 1.1 : 0.9;
    
    // Calculate new zoom level
    double new_zoom = config->zoom_level * zoom_factor;
    
    // Limit zoom levels
    if (new_zoom < 1.0) new_zoom = 1.0;
    if (new_zoom > 20.0) new_zoom = 20.0;
    
    if (new_zoom != config->zoom_level) {
        // Get mouse position in data coordinates
        double mouse_x = event->x;
        double widget_width = gtk_widget_get_allocated_width(widget);
        double range = config->max_x - config->min_x;
        double visible_range = range / config->zoom_level;
        double data_x = config->min_x + (mouse_x / widget_width) * visible_range;
        
        // Update zoom level
        config->zoom_level = new_zoom;
        
        // Adjust pan to zoom toward mouse position
        double new_visible_range = range / config->zoom_level;
        config->pan_offset = (data_x - config->min_x) * (1.0 - (new_visible_range / visible_range));
        
        // Clamp pan offset to valid range
        double max_pan = range * (1.0 - 1.0/config->zoom_level);
        if (config->pan_offset > max_pan) config->pan_offset = max_pan;
        if (config->pan_offset < 0) config->pan_offset = 0;
        
        // Queue redraw
        gtk_widget_queue_draw(widget);
    }
    
    return TRUE;
}
