#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

#include <omnithread.h>
#include <queue>

template<typename T> class Queue
{
    typename std::queue<T> contents;
    omni_mutex mutex;

public:
    inline bool empty() const;
    inline bool pop(T &value);
    inline void push(const T &value);
};

template<typename T> bool Queue<T>::empty() const {
    omni_mutex_lock l(mutex);
    return contents.empty();
};

template<typename T> bool Queue<T>::pop(T &value) {
    omni_mutex_lock l(mutex);
    if(contents.empty())
        return false;
    value = contents.front();
    contents.pop();
    return true;
}

template<typename T> void Queue<T>::push(const T &value) {
    omni_mutex_lock l(mutex);
    contents.push(value);
}

#endif /* ndef QUEUE_H_INCLUDED */
