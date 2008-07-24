#ifndef REFCOUNTINGOBJECT_H_INCLUDED
#define REFCOUNTINGOBJECT_H_INCLUDED

class RefCountingObject
{
    mutable int ref_count;
public:
    inline RefCountingObject();
    virtual ~RefCountingObject() { };
    inline void acquire() const;
    inline void release() const;
};

RefCountingObject::RefCountingObject() : ref_count(1)
{
}

void RefCountingObject::acquire() const
{
    ++ref_count;
}


#ifdef DEBUG
#include <iostream>
#endif

void RefCountingObject::release() const
{
#ifdef DEBUG
    if(this == 0 || ((unsigned long)this) <= 0x0100)
    {
        std::cerr << "WARNING: RefCountingObject::release() called "
                     "with this == " << this << "!" << std::endl;
        return;
    }

    if(ref_count <= 0)
    {
        std::cerr << "WARNING: RefCountingObject::release() called with "
                     "ref_count == " << ref_count << "!" << std::endl;
    }
#endif

    if(--ref_count == 0)
    {
        delete this;
    }
}

#endif /* def REFCOUNTINGOBJECT_H_INCLUDED */
