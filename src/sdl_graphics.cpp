#include "graphics.hpp"
#include "utilities.hpp"
#include <SDL_render.h>
#include <SDL_video.h>

#ifdef SDL_GRAPHICS

#include <SDL.h>

#include <memory>
#include <string>
#include <algorithm>

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

std::unique_ptr<Image> frame;
unique_ptr_fn<SDL_DestroyWindow> window;
unique_ptr_fn<SDL_DestroyRenderer> screen_renderer;
unique_ptr_fn<SDL_DestroyTexture> screen_texture;

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
    (void)c;
    (void)s;
    (void)x;
    (void)y;
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
                    2560,
                    1440,
                    SDL_WINDOW_SHOWN |
                    SDL_WINDOW_ALLOW_HIGHDPI |
                    SDL_WINDOW_RESIZABLE);

    if (window == nullptr)
    {
        print_info("Window could not be created! SDL_Error:"s + SDL_GetError());
        return 1;
    }

    screen_renderer = make_unique_fn<SDL_CreateRenderer, SDL_DestroyRenderer>(
            window.get(), -1, 0);

    if (screen_renderer == nullptr) {
        print_info("Couldn't create renderer:"s + SDL_GetError());
        return 1;
    }

    {
        int w, h;
        SDL_GetRendererOutputSize(screen_renderer.get(), &w, &h);
        resize_camera(w, h);
    }

    SDL_Event event;

	thread compute_thread(main_physics); // start main_physics() in a new thread
	//thread key_thread(key_detection);

	Clock clock;
	double frametime = 1.0;
	clear_console();
	while(running) {
		// main loop ################################################################
		main_graphics();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int w, h;
                    SDL_GetRendererOutputSize(screen_renderer.get(), &w, &h);
                    resize_camera(w, h);
                }
                break;
            }
        }

        SDL_RenderClear(screen_renderer.get());
        SDL_RenderCopy(screen_renderer.get(), screen_texture.get(), nullptr, nullptr);
        SDL_RenderPresent(screen_renderer.get());

        frametime = clock.stop();
		sleep(1.0/(double)camera.fps_limit-frametime);
		clock.start();
		// ##########################################################################
	}
	compute_thread.join();
	//key_thread.join();
	return 0;
}

#endif // SDL_GRAPHICS
