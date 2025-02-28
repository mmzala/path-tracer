#pragma once

#include <memory>
#include <vector>

template<typename T>
struct ResourceHandle
{
    uint32_t handle;
};

template<typename T>
class ResourceManager
{
public:
    ResourceManager() = default;
    const T& Get(ResourceHandle<T> handle) const { return _resources[handle.handle]; }
    const std::vector<T>& GetAll() const { return _resources; }

protected:
    ResourceHandle<T> Create(T&& resource)
    {
        uint32_t index = _resources.size();
        _resources.push_back(std::move(resource));
        return ResourceHandle<T>{ index };
    }

private:
    std::vector<T> _resources {};
};
