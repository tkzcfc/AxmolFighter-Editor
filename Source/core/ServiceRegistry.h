#pragma once

#include "core/IService.h"

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace editor
{
class ServiceRegistry
{
public:
    template <typename T>
    void add(std::shared_ptr<T> service)
    {
        static_assert(std::is_base_of<IService, T>::value, "Service must implement IService.");
        m_services[std::type_index(typeid(T))] = std::move(service);
    }

    template <typename T>
    std::shared_ptr<T> get() const
    {
        const auto it = m_services.find(std::type_index(typeid(T)));
        if (it == m_services.end())
            return nullptr;

        return std::static_pointer_cast<T>(it->second);
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<IService>> m_services;
};
}  // namespace editor
