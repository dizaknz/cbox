#include <SDL.h>
#include <SDL_image.h>

#include "sprite.hpp"

enum Mode {
    Quiet = 0,
    Listen,
    Speak
};

int main(int argc, char ** argv)
{
    bool quit = false;
    SDL_Event event;
    Mode mode = Mode::Quiet;

    // hard coded for sample sprite sheet
    const int width = 194;
    const int height = 120;
    const int numX = 3;
    const int numY = 6;
    const int gapX = 0;
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

    while (!quit)
    {
        while (SDL_PollEvent(&event) != 0)
        {
            switch (event.type)
            {
                case SDL_KEYDOWN:
                case SDL_KEYUP:
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
                        case SDLK_x:
                            std::cout << "Good bye" << std::endl;
                            quit = true;
                            break;
                    }
                    break;
                }
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
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
