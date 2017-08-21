#ifndef NAMESPACE_NAME
#error "Must define NAMESPACE_NAME for type"
#endif

#ifndef OBJECT_NAME
#error "Must define OBJECT_NAME for type"
#endif

#ifndef DEFINITION_FILE
#error "Must define DEFINITION_FILE for type"
#endif

#define STRINGIFY(M)  #M
#define XSTRINGIFY(M) STRINGIFY(M)
#define INCLUDE_FILE  XSTRINGIFY(DEFINITION_FILE)

#include <map>
#include <string>
#include <stdexcept>
#include <boost/variant.hpp>

namespace NAMESPACE_NAME
{
    /**
     * @brief Generic datatype used for generating POD style structs with
     *          member access via strings or tags.
     * @details Uses X-macros extensively to build a struct with a
     *          lookup map by TAG and a lookup man by string. The intent
     *          was to use this for serializer interop where something wants
     *          to retreive a member variable by TAG or by string name.
     *
     *          some_data d{};
     *          d.set(TAG1, value1);
     *          auto x = d.get(TAG1);
     *
     *          std::string y;
     *          d.get("tag1", y);
     */
    class OBJECT_NAME final
    {
    private:
        /// Typedef for our variant type
        /// It's a little scary that this works since we can have
        /// multiple identical template args ... maybe boost::variant
        /// filters via MPL
        using variant = boost::variant<
#           define REFLECT(rt,n,c,d) rt,
#           include INCLUDE_FILE
#           undef REFLECT
            std::nullptr_t // terminator since we cant end in a comma
            >;

        /// Creates properties
#       define REFLECT(rt,n,c,d) rt n = d;
#       include INCLUDE_FILE
#       undef REFLECT

    public:
        /// Item tags
        enum Tag {
#           define REFLECT(rt,n,c,d) c,
#           include INCLUDE_FILE
#           undef REFLECT
        };

        /// Templated get from Tag key
        template<typename T>
            void get(Tag tag, T & fill)
            {
                auto p = mapped_items[tag];
                fill = boost::get<T>(p);
            }

        /// Template get from string
        template<typename T>
            void get(std::string const & s, T & fill)
            {
                auto t = item_lookup[s];
                get(t, fill);
            }

        /// Generic set from variant type
        void set(Tag tag, variant value)
        {
            // set in the map
            mapped_items[tag] = value;

            // set the property
            switch(tag)
            {
#           define REFLECT(rt,n,c,d) case c: n = boost::get<rt>(value); break;
#           include INCLUDE_FILE
#           undef REFLECT
            default:
                throw std::runtime_error("Invalid Tag");
            }
        }

        /// Generic set from STRING
        void set(std::string const & s, variant value)
        {
            auto i = item_lookup[s];
            set(i, std::move(value));
        }

        /// Generates a set-property function for each propery
#       define REFLECT(rt,n,c,d) void set_ ## n(rt val) { set(c, val); }
#       include INCLUDE_FILE
#       undef REFLECT

        /// Generates a getter for each property
#       define REFLECT(rt,n,c,d) rt const & get_ ## n() const { return n; }
#       include INCLUDE_FILE
#       undef REFLECT

    private:
        /// Statically creates a tag->variant map
        std::map<Tag, variant> mapped_items =
        {
#       define REFLECT(rt,n,c,d) { c, rt(d) },
#       include INCLUDE_FILE
#       undef REFLECT
        };

        /// Statically creates a string->tag lookup map
        std::map<std::string, Tag> item_lookup =
        {
#       define REFLECT(rt,n,c,d) { STRINGIFY(n), c },
#       include INCLUDE_FILE
#       undef REFLECT
        };
    };

#undef STRINGIFY
#undef XSTRINGIFY
#undef INCLUDE_FILE
#undef REFLECT
#undef NAMESPACE_NAME
}
