#pragma once
#include <memory>
#include "common.hpp"

class VulkanContext;
class SDL_Window;

class Application
{
public:
    Application();
    ~Application();
    NON_COPYABLE(Application);
    NON_MOVABLE(Application);

    int Run();

private:
    void MainLoopOnce();

    std::unique_ptr<VulkanContext> _vulkanContext;
    SDL_Window* _window = nullptr;
    bool _exitRequested = false;
};