#pragma once 

#include <string>
#include <shared_mutex>

class StringCounter {
public:
    StringCounter() : m_count("0") {}
    
    std::string Get() const 
    {
        std::shared_lock lock(m_mutex);
        return m_count; 
    }
    
    void Reset() 
    {
        std::unique_lock lock(m_mutex);
        m_count = "0";
    }
    
    void operator++()
    {
        std::unique_lock lock(m_mutex);
        Increment();
    }
private:
    void Increment()
    {
        for (int i = m_count.size() - 1; i >= 0; --i) 
        {
            if (m_count[i] < '9') 
            {
                ++m_count[i];
                return;
            } 
            else 
            {
                m_count[i] = '0';
            }
        }
        m_count.insert(0, 1, '1');
    }
    
    mutable std::shared_mutex m_mutex;
    std::string m_count;
};
