#pragma once
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
namespace mp
{
#define make_slot(FUNC,OBJ) mp::easy_bind(FUNC,OBJ)

    size_t easy_hash(const void* addr, const void* obj)
    {
        return reinterpret_cast<size_t>(obj) ^ (*((size_t*)addr));
    }

    template <typename Object, typename ReturnType, typename... Args>
    inline std::pair<size_t, std::function<ReturnType(Args...)>> easy_bind(ReturnType(Object::*const MemPtr)(Args...), Object* const obj)
    {
        size_t key = easy_hash(&MemPtr, obj);
        return std::make_pair(key, [MemPtr, obj](Args... args) { (obj->*MemPtr)(args...); });
    }
    template <typename Object, typename ReturnType, typename... Args>
    inline std::pair<size_t, std::function<ReturnType(Args...)>> easy_bind(ReturnType(Object::*const MemPtr)(Args...) const, const Object* const obj)
    {
        size_t key = easy_hash(&MemPtr, obj);
        return std::make_pair(key, [MemPtr, obj](Args... args) { (obj->*MemPtr)(args...); });
    }

    class connection
    {
    public:
        connection() = default;
        connection(std::function<void(void)>&& deleter)
            : m_deleter(std::move(deleter))
        {
        }
        connection&operator=(std::function<void(void)>&& deleter)
        {
            m_deleter = std::move(deleter);
            return *this;
        }

        void disconnect()
        {
            if (m_deleter)
            {
                m_deleter();
                m_deleter = nullptr;
            }
        }

    private:
        std::function<void(void)> m_deleter;
    };

    class scoped_connection : public connection
    {
    public:
        scoped_connection() = default;
        scoped_connection(connection &&rhs)
        {
            connection::operator=(std::move(rhs));
        }
        scoped_connection& operator=(connection &&rhs)
        {
            disconnect();
            connection::operator=(std::move(rhs));
            return *this;
        }
        scoped_connection(const scoped_connection &rhs) = delete;
        scoped_connection& operator=(const scoped_connection &rhs) = delete;
        scoped_connection(scoped_connection &&rhs)
        {
            connection::operator=(std::move(rhs));
        }
        scoped_connection& operator=(scoped_connection &&rhs)
        {
            disconnect();
            connection::operator=(std::move(rhs));
            return *this;
        }

        ~scoped_connection()
        {
            disconnect();
        }
    };

    template <class ...param_t>
    class signal
    {
    public:
        typedef std::function<void(param_t...)> slot_type;
    private:
        typedef std::map<size_t, slot_type>     slot_list_t;
        std::shared_ptr<slot_list_t>            m_slot_list = std::make_shared<slot_list_t>();
        std::shared_ptr<std::vector<size_t>>    m_keys = std::make_shared<std::vector<size_t>>();
    public:
        void clear()
        {
            m_slot_list.reset(new slot_list_t);
            m_keys.reset(new std::vector<size_t>);
        }

        bool empty() const
        {
            return m_slot_list->empty();
        }

        signal() = default;

        template <typename Object, typename ReturnType>
        inline connection connect(ReturnType(Object::*const MemPtr)(param_t...), Object* const obj)
        {
            return connect(easy_bind(MemPtr, obj));
        }

        template <typename Object, typename ReturnType>
        inline connection connect(ReturnType(Object::*const MemPtr)(param_t...) const, const Object*const obj)
        {
            return connect(easy_bind(MemPtr, obj));
        }

        template <typename Object, typename ReturnType>
        inline void disconnect(ReturnType(Object::*const MemPtr)(param_t...), Object* const obj)
        {
            m_slot_list->erase(easy_hash(&MemPtr, obj));
        }

        template <typename Object, typename ReturnType>
        inline void disconnect(ReturnType(Object::*const MemPtr)(param_t...) const, const Object*const obj)
        {
            m_slot_list->erase(easy_hash(&MemPtr, obj));
        }

        template<typename ...args_t>
        inline void operator() (args_t&&... args) const
        {
            //add ref
            bool erased = false;
            auto slot_list = m_slot_list;
            auto keysref = m_keys;
            auto keys = *keysref;
            for (size_t i = 0; i < keys.size(); ++i)
            {
                auto it = m_slot_list->find(keys[i]);
                if (it != m_slot_list->end())
                {
                    it->second(std::forward<args_t>(args)...);
                }
                else
                {
                    erased = true;
                    (*keysref)[i] = 0;
                }
            }

            if (erased)
            {
                keysref->erase(std::remove(keysref->begin(), keysref->end(), 0), keysref->end());
            }

        }

        connection connect(std::pair<size_t, slot_type>&& slot)
        {
            size_t key = slot.first;
            if (!m_slot_list->emplace(std::move(slot)).second)
            {
                return std::function<void(void)>();
            }
            m_keys->emplace_back(key);
            std::weak_ptr<slot_list_t> wp_slot = m_slot_list;
            return std::function<void(void)>(
                [wp_slot, key]{
                auto slot_list = wp_slot.lock();
                if (slot_list)
                    slot_list->erase(key);
            });
        }

//         connection connect(slot_type&& slot,size_t key)
//         {
//             if (!m_slot_list->emplace(key, std::move(slot)).second)
//             {
//                 return std::function<void(void)>();
//             }
//             m_keys->emplace_back(key);
//             std::weak_ptr<slot_list_t> wp_slot = m_slot_list;
//             return std::function<void(void)>(
//                 [wp_slot, key]{
//                 auto slot_list = wp_slot.lock();
//                 if (slot_list)
//                     slot_list->erase(key);
//             });
//         }
    };
} // namespace

#include <iostream>
#include <stdio.h>
class CCC
{
public:
    void fn(const char* var)
    {
        printf("%s CCC::fn\n", var);
    }
    void fn1(const char* var)
    {
        printf("%s CCC::fn1\n", var);
    }
private:
    int bb;
};

int main(int argc, char * argv[])
{
    mp::signal<const char*> sig;
    CCC a;
    {
        mp::scoped_connection conn = sig.connect(&CCC::fn1, &a);
        auto conn2 = sig.connect(&CCC::fn, &a);
        sig("step 1");
        conn2.disconnect();
        sig("step 2");
        sig.connect(&CCC::fn, &a);
        sig("step 3");
        sig.disconnect(&CCC::fn, &a);
        sig("step 4");
    }
    sig("step 5");

	return 0;

}