#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <atomic>
#include <iostream>
#include <ostream>
#include <string>

class SpeakingSprite
{
    private:
        SDL_Renderer *renderer;
        SDL_Surface *image;
        SDL_Texture *texture;
        std::string source;
        int width;
        int height;
        int numX;
        int numY;
        int num;
        int gapX;
        int gapY;
        int quietIdx;
        int listenIdx;
        std::atomic<bool> loaded = { false };
        std::atomic<Uint32> currentIdx = { 0 };
    private:
        int x(Uint32 idx)
        {
            return idx % numX;
        }

        int y(Uint32 idx)
        {
            return int(idx / numX);
        }

        void render(Uint32 idx)
        {
            int c = x(idx);
            int r = y(idx);

            SDL_Rect src = { 
                (width * c) + (gapX * c),
                (height * r) + (gapY * r),
                width,
                height
            };
            SDL_Rect dest = {
                0,
                0, 
                width, 
                height
            };

            SDL_RenderClear(renderer);
            auto ret = SDL_RenderCopy(renderer, texture, &src, &dest);
            if (ret < 0)
            {
                std::cerr << "ERROR: Fail to copy texture, SDL error: " << SDL_GetError() << std::endl;
                return;
            }
            SDL_RenderPresent(renderer);

            currentIdx = idx;
        }
    public:
        SpeakingSprite(
            SDL_Renderer *renderer,
            const std::string &source,
            int width,
            int height,
            int numX,
            int numY,
            int gapX,
            int gapY,
            int quietIdx,
            int listenIdx) 
            : renderer(renderer),
              source(source),
              width(width),
              height(height),
              numX(numX),
              numY(numY),
              gapX(gapX),
              gapY(gapY),
              quietIdx(quietIdx),
              listenIdx(listenIdx)
        {
            num = numX * numY;
        }

        virtual ~SpeakingSprite()
        {
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(image);
        }

        bool Load()
        {
            image = IMG_Load(source.c_str());
            if (image == nullptr)
            {
                std::cerr << "ERROR: Could not load spritesheet: "
                         << source 
                         << ", SDL error: " 
                         << SDL_GetError() 
                         << std::endl;
                return false;
            }
            texture = SDL_CreateTextureFromSurface(renderer, image);
            if (texture == nullptr)
            {
                std::cerr << "ERROR: Failed to create texture from surface, SDL error: "
                          << SDL_GetError()
                          << std::endl;
                return false;
            }
            loaded = true;

            return loaded;
        }
        
        bool IsLoaded()
        {
            return loaded;
        }

        void BeQuiet()
        {
            render(quietIdx);
        }

        void Listen()
        {
            render(listenIdx);
        }

        void Speak()
        {
            Uint32 ticks = SDL_GetTicks();
            Uint32 idx = (ticks / 100) % num;
            render(idx);
        }

        void Current()
        {
            render(currentIdx);
        }

        void Previous()
        {
            render(currentIdx == 0 ? num - 1 : currentIdx - 1);
        }

        void Next()
        {
            render(currentIdx == num - 1 ? 0 : currentIdx + 1);
        }
};

