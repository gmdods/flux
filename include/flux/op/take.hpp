
// Copyright (c) 2022 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef FLUX_OP_TAKE_HPP_INCLUDED
#define FLUX_OP_TAKE_HPP_INCLUDED

#include <flux/core.hpp>
#include <flux/op/for_each_while.hpp>
#include <flux/op/slice.hpp>

namespace flux {

namespace detail {

template <sequence Base>
struct take_adaptor : inline_sequence_base<take_adaptor<Base>>
{
private:
    Base base_;
    distance_t count_;

public:
    constexpr take_adaptor(decays_to<Base> auto&& base, distance_t count)
        : base_(FLUX_FWD(base)),
          count_(count)
    {}

    [[nodiscard]] constexpr auto base() const& -> Base const& { return base_; }
    [[nodiscard]] constexpr auto base() && -> Base&& { return std::move(base_); }

    struct flux_sequence_traits {
    private:
        struct cursor_type {
            cursor_t<Base> base_cur;
            distance_t length;

            friend bool operator==(cursor_type const&, cursor_type const&) = default;
            friend auto operator<=>(cursor_type const& lhs, cursor_type const& rhs) = default;
        };

    public:
        using value_type = value_t<Base>;

        static constexpr auto first(auto& self) -> cursor_type
        {
            return cursor_type{.base_cur = flux::first(self.base_),
                               .length = self.count_};
        }

        static constexpr auto is_last(auto& self, cursor_type const& cur) -> bool
        {
            return cur.length <= 0 || flux::is_last(self.base_, cur.base_cur);
        }

        static constexpr auto inc(auto& self, cursor_type& cur)
        {
            cur.length = num::checked_sub(cur.length, distance_t{1});
            if constexpr (multipass_sequence<Base>) {
                 flux::inc(self.base_, cur.base_cur);
            } else {
                 if (cur.length > 0)
                     flux::inc(self.base_, cur.base_cur);
            }
        }

        static constexpr auto read_at(auto& self, cursor_type const& cur)
            -> decltype(flux::read_at(self.base_, cur.base_cur))
        {
            return flux::read_at(self.base_, cur.base_cur);
        }

        static constexpr auto move_at(auto& self, cursor_type const& cur)
            -> decltype(flux::move_at(self.base_, cur.base_cur))
        {
            return flux::move_at(self.base_, cur.base_cur);
        }

        static constexpr auto read_at_unchecked(auto& self, cursor_type const& cur)
            -> decltype(flux::read_at_unchecked(self.base_, cur.base_cur))
        {
            return flux::read_at_unchecked(self.base_, cur.base_cur);
        }

        static constexpr auto move_at_unchecked(auto& self, cursor_type const& cur)
            -> decltype(flux::move_at_unchecked(self.base_, cur.base_cur))
        {
            return flux::move_at_unchecked(self.base_, cur.base_cur);
        }

        static constexpr auto dec(auto& self, cursor_type& cur)
            requires bidirectional_sequence<Base>
        {
            cur.length = num::checked_add(cur.length, distance_t{1});
            flux::dec(self.base_, cur.base_cur);
        }

        static constexpr auto inc(auto& self, cursor_type& cur, distance_t offset)
            requires random_access_sequence<Base>
        {
            cur.length = num::checked_sub(cur.length, offset);
            flux::inc(self.base_, cur.base_cur, offset);
        }

        static constexpr auto distance(auto& self, cursor_type const& from, cursor_type const& to)
            -> distance_t
            requires random_access_sequence<Base>
        {
            return (std::min)(flux::distance(self.base_, from.base_cur, to.base_cur),
                              num::checked_sub(from.length, to.length));
        }

        static constexpr auto data(auto& self)
            -> decltype(flux::data(self.base_))
            requires contiguous_sequence<Base>
        {
            return flux::data(self.base_);
        }

        static constexpr auto size(auto& self)
            requires sized_sequence<Base> || infinite_sequence<Base>
        {
            if constexpr (infinite_sequence<Base>) {
                return self.count_;
            } else {
                return (std::min)(flux::size(self.base_), self.count_);
            }
        }

        static constexpr auto last(auto& self) -> cursor_type
            requires (random_access_sequence<Base> && sized_sequence<Base>) ||
                      infinite_sequence<Base>
        {
            return cursor_type{
                .base_cur = flux::next(self.base_, flux::first(self.base_), size(self)),
                .length = 0
            };
        }

        static constexpr auto for_each_while(auto& self, auto&& pred) -> cursor_type
        {
            distance_t len = self.count_;
	        if (len == 0) return first(self);
            auto cur = flux::for_each_while(self.base_, [&](auto&& elem) {
                return std::invoke(pred, FLUX_FWD(elem)) && (--len > 0);
            });

            if constexpr (multipass_sequence<Base>)
		        if (len == 0)
			        flux::inc(self.base_, cur);
            return cursor_type{.base_cur = std::move(cur), .length = len};
        }
    };
};

struct take_fn {
    template <adaptable_sequence Seq>
    [[nodiscard]]
    constexpr auto operator()(Seq&& seq, std::integral auto count) const
    {
        auto count_ = checked_cast<distance_t>(count);
        if (count_ < 0) {
            runtime_error("Negative argument passed to take()");
        }

        return take_adaptor<std::decay_t<Seq>>(FLUX_FWD(seq), count_);
    }
};

} // namespace detail

FLUX_EXPORT inline constexpr auto take = detail::take_fn{};

template <typename Derived>
constexpr auto inline_sequence_base<Derived>::take(std::integral auto count) &&
{
    return flux::take(std::move(derived()), count);
}

} // namespace flux

#endif // FLUX_OP_TAKE_HPP_INCLUDED
