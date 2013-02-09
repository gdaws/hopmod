/*
 * The Fungu Scripting Engine
 *
 * (C) Copyright 2008-2009 Graham Daws
 * (C) Copyright 2005 Christopher Diggins
 * (C) Copyright 2005 Pablo Aguilar
 * (C) Copyright 2001 Kevlin Henney
 *
 *  Boost Software License - Version 1.0 - August 17th, 2003
 *
 *  Permission is hereby granted, free of charge, to any person or organization
 *  obtaining a copy of the software and accompanying documentation covered by
 *  this license (the "Software") to use, reproduce, display, distribute,
 *  execute, and transmit the Software, and to prepare derivative works of the
 *  Software, and to permit third-parties to whom the Software is furnished to
 *  do so, all subject to the following:
 *
 *  The copyright notices in the Software and this entire statement, including
 *  the above license grant, this restriction and the following disclaimer,
 *  must be included in all copies of the Software, in whole or in part, and
 *  all derivative works of the Software, unless such copies or derivative
 *  works are solely in the form of machine-executable object code generated by
 *  a source language processor.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 *  SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 *  FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */
#ifndef FUNGU_SCRIPT_ANY_HPP
#define FUNGU_SCRIPT_ANY_HPP

#include "../string.hpp"
#include "type_id.hpp"
#include "cast.hpp"

#ifdef FUNGU_WITH_LUA
#include "lua/push_value.hpp"
#endif

#include <stdexcept>
#include <typeinfo>
#include <algorithm>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/is_arithmetic.hpp>

namespace fungu{
namespace script{

template<typename Target,typename Source>
Target lexical_cast(const Source &);  //Forward declaration

struct bad_any_cast: std::bad_cast
{
    bad_any_cast(const std::type_info& src, const std::type_info& dest);
    const char* what();
    const char* from;
    const char* to;
};

namespace any_detail {

// function pointer table

struct fxn_ptr_table {
  const std::type_info& (*get_type)();
  type_id (*get_type_id)();
  void (*static_delete)(void**);
  void (*clone)(void* const*, void**);
  void (*move)(void* const*,void**);
  const_string (*to_string)(void * const *);
#ifdef FUNGU_WITH_LUA
  void (*lua_push_value)(lua_State *,void * const *);
#endif
};

// static functions for small value-types

template<bool is_small>
struct fxns
{
  template<typename T>
  struct type {
    static const std::type_info& get_type() {
      return typeid(T);
    }
    static type_id get_type_id() {
      return type_id::get(type_tag<T>());
    }
    static void static_delete(void** x) {
      reinterpret_cast<T*>(reinterpret_cast<void *>(x))->~T();
    }
    static void clone(void* const* src, void** dest) {
      new(dest) T(*reinterpret_cast<T const*>(src));
    }
    static void move(void* const* src, void** dest) {
      reinterpret_cast<T*>(reinterpret_cast<void *>(dest))->~T();
      new(reinterpret_cast<T*>(reinterpret_cast<void *>(dest))) T(*reinterpret_cast<T const*>(src));
    }
    static const_string to_string(void *const* src) {
        return fungu::script::lexical_cast<const_string,T>(*reinterpret_cast<const T *>(src));
    }
    #ifdef FUNGU_WITH_LUA
    static void lua_push_value(lua_State * L, void * const * src)
    {
        lua::push_value(L, *reinterpret_cast<T const *>(src));
    }
    #endif
  };
};

// static functions for big value-types (bigger than a void*)

template<>
struct fxns<false>
{
  template<typename T>
  struct type {
    static const std::type_info& get_type() {
      return typeid(T);
    }
    static type_id get_type_id() {
      return type_id::get(type_tag<T>());
    }
    static void static_delete(void** x) {
        delete(*reinterpret_cast<T**>(x));
    }
    static void clone(void* const* src, void** dest) {
      *dest = new T(**reinterpret_cast<T* const*>(src));
    }
    static void move(void* const* src, void** dest) {
      (*reinterpret_cast<T**>(dest))->~T();
      new(*reinterpret_cast<T**>(dest)) T(**reinterpret_cast<T* const*>(src));
    }
    static const_string to_string(void *const* src) {
        return fungu::script::lexical_cast<const_string>(**reinterpret_cast<T * const *>(src));
    }
    #ifdef FUNGU_WITH_LUA
    static void lua_push_value(lua_State * L, void * const * src)
    {
        lua::push_value(L, ** reinterpret_cast<T * const *>(src));
    }
    #endif
  };
};

template<typename T>
struct get_table
{
  static const bool is_small = sizeof(T) <= sizeof(void*);

  static fxn_ptr_table* get()
  {
    static fxn_ptr_table static_table = {
      fxns<is_small>::template type<T>::get_type
    , fxns<is_small>::template type<T>::get_type_id
    , fxns<is_small>::template type<T>::static_delete
    , fxns<is_small>::template type<T>::clone
    , fxns<is_small>::template type<T>::move
    , fxns<is_small>::template type<T>::to_string
    #ifdef FUNGU_WITH_LUA
    , fxns<is_small>::template type<T>::lua_push_value
    #endif
    };
    return &static_table;
  }
};

struct empty {};

} // namespace any_detail

struct any
{
    template <typename T>
    any(const T& x) {
      table = any_detail::get_table<T>::get();
      if (sizeof(T) <= sizeof(void*)) {
        new(&object) T(x);
      }
      else {
        object = new T(x);
      }
    }
    
    any();
    any(const any& x);
    
    ~any();
    
    any& assign(const any& x);

    template <typename T>
    any& assign(const T& x)
    {
      // are we copying between the same type?
      any_detail::fxn_ptr_table* x_table = any_detail::get_table<typename boost::remove_const<T>::type>::get();
      if (table == x_table) {
        // if so, we can avoid deallocating and resuse memory
        if (sizeof(T) <= sizeof(void*)) {
          void ** ptr = &object; //fixes strict-aliasing warning
          reinterpret_cast<T*>(reinterpret_cast<void *>(ptr))->~T();
          // create copy on-top of object pointer itself
          new(&object) T(x);
        }
        else {
          reinterpret_cast<T*>(object)->~T();
          // create copy on-top of old version
          new(object) T(x);
        }
      }
      else {
        reset();
        if (sizeof(T) <= sizeof(void*)) {
          // create copy on-top of object pointer itself
          new(&object) T(x);
          // update table pointer
          table = x_table;
        }
        else {
          object = new T(x);
          table = x_table;
        }
      }
      return *this;
    }

    template<typename T>
    any& operator=(T const& x) {
      return assign(x);
    }
    
    any& operator=(const any & x);
    
    // utility functions
    
    any& swap(any& x);
    
    const std::type_info& get_type()const;
    type_id get_type_id()const;
    
    const_string to_string()const;
        
    #ifdef FUNGU_WITH_LUA
    void push_value(lua_State *)const;
    #endif
    
    bool empty()const;

    void reset();
    
    static any null_value();
    
    // fields
    any_detail::fxn_ptr_table* table;
    void* object;
};

// boost::any-like casting

template<typename T>
T* any_cast(any* this_)
{
    static const bool SMALL_TYPE = sizeof(T) <= sizeof(void*);
    void * source_object = (SMALL_TYPE ? &this_->object : this_->object);
    
    // If types don't match then try type coercion
    if (this_->get_type() != typeid(T)) {
        
        type_id destType;
        destType = type_id::get(type_tag<T>());
        if(destType.base() == type_id::FOREIGN) return NULL;
        
        try
        {
            static T temporary_destination;
            cast(source_object, this_->get_type_id(), &temporary_destination, destType);
            return &temporary_destination;
        }
        catch(std::bad_cast)
        {
            return NULL;
        }
    }
    
    return reinterpret_cast<T*>(source_object);
}

template<typename T>
T const* any_cast(any const* this_) {
    return any_cast<T>(const_cast<any*>(this_));
}

template<typename T>
T const& any_cast(any const& this_){
    T * ptr = any_cast<T>(const_cast<any*>(&this_));
    if(!ptr) throw bad_any_cast(this_.get_type(), typeid(T));
    return *ptr;
}

bool any_is_string(const any &);

} //namespace script
} //namespace fungu

#endif