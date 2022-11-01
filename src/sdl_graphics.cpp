#include "graphics.hpp"
#include "utilities.hpp"

#ifdef SDL_GRAPHICS

#include "SDL_FontCache.h"
#include "info.hpp"
#include "lbm.hpp"

#include <SDL.h>
#include <SDL_render.h>
#include <SDL_video.h>

#include <memory>
#include <string>
#include <algorithm>
#include <vector>

using namespace std::literals;

namespace
{

template<typename>
struct param_type;

template<typename Ret, typename T>
struct param_type<Ret(*)(T*)>
{
    using type = T;
};

template<typename T, auto DestroyFn>
struct fn_deleter
{
    void operator()(T* ptr) const noexcept(noexcept(DestroyFn(nullptr)))
    {
        DestroyFn(ptr);
    }
};

template<auto CreateFn, auto DestroyFn, typename... CreateArgs>
auto make_unique_fn(CreateArgs&&... args)
{
    using ptr_t = std::remove_pointer_t<decltype(CreateFn(std::forward<CreateArgs>(args)...))>;
    return std::unique_ptr<ptr_t, fn_deleter<ptr_t, DestroyFn>>{
        CreateFn(std::forward<CreateArgs>(args)...)
    };
}

template<auto DestroyFn>
using unique_ptr_fn = std::unique_ptr<
    typename param_type<decltype(DestroyFn)>::type,
    fn_deleter<typename param_type<decltype(DestroyFn)>::type, DestroyFn>>;

struct Label
{
    std::string string;
    int x, y;
    Color color;
};

std::unique_ptr<Image> frame;
unique_ptr_fn<SDL_DestroyWindow> window;
unique_ptr_fn<SDL_DestroyRenderer> screen_renderer;
unique_ptr_fn<SDL_DestroyTexture> screen_texture;
unique_ptr_fn<FC_FreeFont> font;
std::vector<Label> labels;

void resize_camera(unsigned width, unsigned height)
{
    auto round_to_8 = [](unsigned dim)
    {
        unsigned const quotient = dim / 8;
        unsigned const remainder = dim % 8;
        return quotient * 8 + (remainder != 0);
    };

    screen_texture = make_unique_fn<SDL_CreateTexture, SDL_DestroyTexture>(
        screen_renderer.get(),
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        round_to_8(width),
        round_to_8(height)
    );

    camera.fps_limit = 60u; // find out screen refresh rate
	camera.width  = round_to_8(width);  // must be divisible by 8
	camera.height = round_to_8(height); // must be divisible by 8
	camera.fov = 100.0f;
	set_zoom(1.0f);
	camera.update_matrix();
	frame.reset(new Image(camera.width, camera.height));
}

} // namespace

void draw_bitmap(const void* buffer)
{
    void *pixels;
    int pitch, width, height;
    SDL_QueryTexture(screen_texture.get(), nullptr, nullptr, &width, &height);
    SDL_LockTexture(screen_texture.get(), nullptr, &pixels, &pitch);

    std::copy_n(reinterpret_cast<uint32_t const*>(buffer),
                width * height,
                reinterpret_cast<uint32_t*>(pixels));

    SDL_UnlockTexture(screen_texture.get());
}

void draw_label(const Color& c, const string& s, const int x, const int y)
{
    labels.emplace_back(Label{s, x, y, c});
}

void set_cursor_pos(int x, int y)
{
    (void)x;
    (void)y;
}

int main(int argc, char* argv[]) {
	main_arguments = get_main_arguments(argc, argv);

    window = make_unique_fn<SDL_CreateWindow, SDL_DestroyWindow>(
                    WINDOW_NAME,
                    SDL_WINDOWPOS_UNDEFINED,
                    SDL_WINDOWPOS_UNDEFINED,
                    640,
                    480,
                    SDL_WINDOW_SHOWN |
                    SDL_WINDOW_RESIZABLE);

    if (window == nullptr)
    {
        print_info("Window could not be created! SDL_Error:"s + SDL_GetError());
        return 1;
    }

    float font_scale = 1;
    if (SDL_Rect rect; SDL_GetDisplayUsableBounds(
                SDL_GetWindowDisplayIndex(window.get()), &rect) == 0)
    {
        font_scale = static_cast<float>(rect.w) / 1920.0f;
        SDL_SetWindowSize(window.get(), (rect.w * 2) / 3, (rect.h * 2) / 3);
        SDL_SetWindowPosition(window.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    screen_renderer = make_unique_fn<SDL_CreateRenderer, SDL_DestroyRenderer>(
            window.get(), -1, 0);

    if (screen_renderer == nullptr)
    {
        print_info("Couldn't create renderer:"s + SDL_GetError());
        return 1;
    }

    font = make_unique_fn<FC_CreateFont, FC_FreeFont>();

    if (font == nullptr)
    {
        print_info("Couldn't create font:"s + SDL_GetError());
        return 1;
    }

    if (!FC_LoadFont(
            font.get(),
            screen_renderer.get(),
            (get_exe_path() + "../../fonts/RobotoMono-Regular.ttf").c_str(),
            12 * font_scale,
            FC_MakeColor(255,255,255,255),
            TTF_STYLE_NORMAL))
    {
        print_info("Couldn't load font:"s + SDL_GetError());
        return 1;
    }

    global_font_height = 12 * font_scale;
    global_font_width = 7 * font_scale;

    {
        int w, h;
        SDL_GetRendererOutputSize(screen_renderer.get(), &w, &h);
        resize_camera(w, h);
    }

    SDL_Event event;

	thread compute_thread(main_physics); // start main_physics() in a new thread

	Clock clock;
	double frametime = 1.0;
	clear_console();
	while(running) {
		// main loop ################################################################
		main_graphics();

        main_label(frametime);

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    if (!info.lbm) break;
                    int w, h;
                    SDL_GetRendererOutputSize(screen_renderer.get(), &w, &h);
                    resize_camera(w, h);
                    info.lbm->reallocate_graphics();
                }
                break;
            }
        }

        SDL_RenderClear(screen_renderer.get());
        SDL_RenderCopy(screen_renderer.get(), screen_texture.get(), nullptr, nullptr);
        for (auto const& label : labels)
        {
            FC_Draw(font.get(), screen_renderer.get(), label.x, label.y, label.string.c_str());
        }
        labels.clear();
        SDL_RenderPresent(screen_renderer.get());

        frametime = clock.stop();
		sleep(1.0/(double)camera.fps_limit-frametime);
		clock.start();
		// ##########################################################################
	}
	compute_thread.join();
	return 0;
}

#endif // SDL_GRAPHICS
