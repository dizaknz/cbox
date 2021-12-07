#include <SDL.h>
#include <SDL_image.h>
#include <iomanip>

#include "sprite.hpp"

enum Mode {
    Quiet = 0,
    Listen,
    Speak,
    Current,
    Previous,
    Next
};

void intro()
{
    std::cout << std::setfill('-') << std::setw(50) << "-\n"
              << "Sprite animation demo\n\n"
              << "Keys:\n"
              << "  q - quiet\n"
              << "  l - listen\n"
              << "  s - speak (animate)\n"
              << "  p - previous (hold to animate backwards)\n"
              << "  n - next (hold to animate forwards)\n"
              << "  x - quit\n"
              << std::endl;
}

int main(int argc, char ** argv)
{
    bool quit = false;
    SDL_Event event;
    Mode mode = Mode::Quiet;

    // hard coded for sample sprite sheet
    const int width = 192;
    const int height = 120;
    const int numX = 3;
    const int numY = 6;
    const int gapX = 3;
    const int gapY = 1;
    const int quietIdx = 0;
    const int listenIdx = 2;

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window *window = SDL_CreateWindow(
        "Sprite Animation",
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED,
        width, 
        height, 
        0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_SetRenderDrawColor(renderer, 168, 230, 255, 255);
    SDL_RenderClear(renderer);

    const std::string sheet = "spritesheet.png";
    auto sprite = SpeakingSprite(
        renderer,
        sheet,
        width,
        height,
        numX,
        numY,
        gapX,
        gapY,
        quietIdx,
        listenIdx);

    if (!sprite.Load())
    {
        exit(1);
    }

    intro();

    while (!quit)
    {
        while (SDL_PollEvent(&event) != 0)
        {
            switch (event.type)
            {
                case SDL_KEYDOWN:
                {
                    switch(event.key.keysym.sym)
                    {
                        case SDLK_q:
                            mode = Mode::Quiet;
                            break;
                        case SDLK_l:
                            mode = Mode::Listen;
                            break;
                        case SDLK_s:
                            mode = Mode::Speak;
                            break;
                        case SDLK_p:
                        case SDLK_LEFT:
                            mode = Mode::Previous;
                            break;
                        case SDLK_n:
                        case SDLK_RIGHT:
                            mode = Mode::Next;
                            break;
                        case SDLK_x:
                            std::cout << "Good bye" << std::endl;
                            quit = true;
                            break;
                    }
                    break;
                }
                case SDL_KEYUP:
                    switch(event.key.keysym.sym)
                    {
                        case SDLK_p:
                        case SDLK_LEFT:
                        case SDLK_n:
                        case SDLK_RIGHT:
                            mode = Mode::Current;
                            break;
                    }
                    break;
                case SDL_QUIT:
                    quit = true;
                    break;
            }
        }

        switch (mode)
        {
            case Mode::Quiet:
                sprite.BeQuiet();
                break;
            case Mode::Listen:
                sprite.Listen();
                break;
            case Mode::Speak:
                sprite.Speak();
                break;
            case Mode::Current:
                sprite.Current();
                break;
            case Mode::Previous:
                sprite.Previous();
                break;
            case Mode::Next:
                sprite.Next();
                break;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
