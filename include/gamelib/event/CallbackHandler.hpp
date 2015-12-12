#ifndef CALLBACK_HANDLER_HPP
#define CALLBACK_HANDLER_HPP

#include <vector>
#include <algorithm>
#include <utility>

namespace gamelib
{
    template <class Ret, class... Args>
    class CallbackHandler
    {
        public:
            typedef Ret (*CallbackFunction)(void*, Args...);

        private:
            struct CallbackInfo
            {
                void* me;
                CallbackFunction callback;
                bool remove;

                CallbackInfo(void* me_, CallbackFunction callback_) :
                    me(me_),
                    callback(callback_),
                    remove(false)
                {}

                bool operator==(const CallbackInfo& ci) const
                {
                    return me == ci.me && callback == ci.callback && remove == ci.remove;
                }
            };

        public:
            void regCallback(CallbackFunction callback, void* me);

            // The entry won't be erased immediatelly, because it could damage the iterators in call().
            // Instead it will be erased when calling clean() or when iterating over it inside call().
            void unregCallback(CallbackFunction callback, void* me);

            template <class... Args2>
            void call(Args2&&... args);

            void clear();

            // Iterates through the list and removes every entry marked for removal
            // DON'T CALL THIS INSIDE CALL() OR A KITTEN WILL DIE AND SEGFAULTS MAY RAIN UPON YOU!
            void clean();

            // Might not always report the correct size, because there could still be entrys marked for removal, that haven't been removed yet.
            size_t size() const;

        private:
            std::vector<CallbackInfo> _callbacks;
    };
}

#include "CallbackHandler.inl"

#endif
