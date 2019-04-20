#define _CRT_SECURE_NO_WARNINGS 1

#include <string.h>
#include "nuklear-glfw-vulkan.h"
#include "overlay.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

#define UNUSED(a) (void)a
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a)/sizeof(a)[0])

GLFWwindow* win;
struct nk_context *ctx;
struct nk_color background;
struct nk_font_atlas *atlas;

void init_overlay(GLFWwindow* _win, VkDevice logical_device, VkPhysicalDevice physical_device, VkQueue graphics_queue, uint32_t graphics_queue_index, VkFramebuffer* framebuffers, uint32_t framebuffers_len, VkFormat color_format, VkFormat depth_format) {
    win = _win;
    ctx = nk_glfw3_init(win, logical_device, physical_device, graphics_queue, graphics_queue_index, framebuffers, framebuffers_len, color_format, depth_format, NK_GLFW3_INSTALL_CALLBACKS);
    // /* Load Fonts: if none of these are loaded a default font will be used  */
    // /* Load Cursor: if you uncomment cursor loading please hide the cursor */
    {

        nk_glfw3_font_stash_begin(&atlas);
    /*struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0);*/
    /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
    /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
    /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
    /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
    /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
        nk_glfw3_font_stash_end();
    /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
    /*nk_style_set_font(ctx, &droid->handle);*/

    /* style.c */
    /*set_style(ctx, THEME_WHITE);*/
    /*set_style(ctx, THEME_RED);*/
    /*set_style(ctx, THEME_BLUE);*/
    /*set_style(ctx, THEME_DARK);*/
    }
}

VkSemaphore submit_overlay(struct overlay_settings* settings, uint32_t buffer_index, VkSemaphore main_finished_semaphore) {
    struct nk_color background = nk_rgb(
        (int) (settings->bg_color[0] * 255),
        (int) (settings->bg_color[1] * 255),
        (int) (settings->bg_color[2] * 255)
    );

        /* Input */
        glfwPollEvents();
        nk_glfw3_new_frame();

        /* GUI */
        if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {

            nk_layout_row_static(ctx, 30, 80, 1);
            if (nk_button_label(ctx, "button"))
                fprintf(stdout, "button pressed\n");

            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_option_label(ctx, "up", settings->orientation == UP)) settings->orientation = UP;
            if (nk_option_label(ctx, "down", settings->orientation == DOWN)) settings->orientation = DOWN;

            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_int(ctx, "Zoom:", 0, &settings->zoom, 100, 10, 1);

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "background:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_combo_begin_color(ctx, background, nk_vec2(nk_widget_width(ctx),400))) {
                nk_layout_row_dynamic(ctx, 120, 1);
                background = nk_color_picker(ctx, background, NK_RGBA);
                nk_layout_row_dynamic(ctx, 25, 1);
                background.r = (nk_byte)nk_propertyi(ctx, "#R:", 0, background.r, 255, 1,1);
                background.g = (nk_byte)nk_propertyi(ctx, "#G:", 0, background.g, 255, 1,1);
                background.b = (nk_byte)nk_propertyi(ctx, "#B:", 0, background.b, 255, 1,1);
                background.a = (nk_byte)nk_propertyi(ctx, "#A:", 0, background.a, 255, 1,1);
                nk_combo_end(ctx);
            }
        }
        nk_end(ctx);

        /* -------------- EXAMPLES ---------------- */
        /*calculator(ctx);*/
        /*overview(ctx);*/
        /*node_editor(ctx);*/
        /* ----------------------------------------- */

        /* Draw */
        // {float bg[4];
        // nk_color_fv(bg, background);
        // glfwGetWindowSize(win, &width, &height);
        // glViewport(0, 0, width, height);
        // glClear(GL_COLOR_BUFFER_BIT);
        // glClearColor(bg[0], bg[1], bg[2], bg[3]);
        // /* IMPORTANT: `nk_glfw_render` modifies some global OpenGL state
        //  * with blending, scissor, face culling, depth test and viewport and
        //  * defaults everything back into a default state.
        //  * Make sure to either a.) save and restore or b.) reset your own state after
        //  * rendering the UI. */
        // nk_glfw3_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        // glfwSwapBuffers(win);}
    // }
    
    // glfwTerminate();
    // return 0;
    
    nk_color_fv(settings->bg_color, background);
    return nk_glfw3_render(NK_ANTI_ALIASING_ON, buffer_index, main_finished_semaphore);
}

void shutdown_overlay() {
    nk_glfw3_shutdown();
}