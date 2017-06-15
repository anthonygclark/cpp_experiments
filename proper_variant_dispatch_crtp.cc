#include <iostream>
#include <boost/variant.hpp>

struct message { int x; };
using Variant = boost::variant<message, std::nullptr_t>;

template<typename T, typename D = void>
struct message_handler {
    // default message handler
    void operator()(message m) { std::cout << __PRETTY_FUNCTION__ << std::endl; }

    // callback from wherever gets a message over a wire
    void handle_message(T m) {
        // compile-time check for intercept_message
        constexpr void (D::*intercept_message_caller)(T) = &D::intercept_message;
        // call it
        (static_cast<D *>(this)->*intercept_message_caller)(m);
    }
};

// specialization of handle_message for default case of 
// T=message, D=void
template<>
void message_handler<message, void>::handle_message(message m) {
    operator()(m);
}

struct message_handler_2 : message_handler<Variant, message_handler_2>, boost::static_visitor<void>
{
    // to make boost::static_visitor happy
    using message_handler::operator();

    void operator()(std::nullptr_t x) { std::cout << __PRETTY_FUNCTION__ << std::endl; }
    void intercept_message(Variant v) { boost::apply_visitor(*this, v); }
};


int main()
{
    message_handler<message> h;
    h.handle_message(message{0}); // should call message_handler::handle_message(message)

    message_handler_2 hh;
    hh.handle_message(Variant{message{1}}); // should also call message_handler::handle_message(message)
    hh.handle_message(Variant{nullptr}); // should call message_handler_2::operator(nullptr)
}
