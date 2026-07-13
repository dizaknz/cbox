#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_surface.h>
#include <atomic>
#include <iostream>
#include <ostream>
#include <string>

class SpeakingSprite
{
    private:
        SDL_Renderer *renderer;
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

            SDL_FRect src = { 
                (float)((width * c) + (gapX * c)),
                (float)((height * r) + (gapY * r)),
                (float)(width),
                (float)(height)
            };
            const SDL_FRect* src_ptr = &src;
            SDL_FRect dest = {
                0.0f,
                0.0f, 
                (float)(width),
                (float)(height)
            };
            const SDL_FRect* dest_ptr = &dest;

            SDL_RenderClear(renderer);
            auto ret = SDL_RenderTexture(renderer, texture, src_ptr, dest_ptr);
            if (!ret)
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
        }

        bool Load()
        {
            texture = IMG_LoadTexture(renderer, source.c_str());
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
            Uint64 ticks = SDL_GetTicks();
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

