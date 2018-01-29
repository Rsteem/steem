#ifndef GOLOS_HARDFORK_OBJECT_HPP
#define GOLOS_HARDFORK_OBJECT_HPP

#include <golos/chain/steem_object_types.hpp>

#include <golos/protocol/types.hpp>

#include <golos/version/version.hpp>

namespace golos {
    namespace chain {

        /**
         * @brief Tracks hardforks properties
         * @ingroup objects
         */

        class hardfork_property_object : public object<hardfork_property_object_type, hardfork_property_object> {
        public:
            template<typename Constructor, typename Allocator>
            hardfork_property_object(Constructor &&c, allocator<Allocator> a)
                    :processed_hardforks(a.get_segment_manager()) {
                c(*this);
            }

            id_type id;

            boost::interprocess::vector<fc::time_point_sec, allocator<fc::time_point_sec>> processed_hardforks; ///< Used for storing the processed hardforks timestamps
            uint32_t last_hardfork = 0; ///< Latest hardfork applied timestamp
            protocol::hardfork_version current_hardfork_version; ///< Current hardfork daemon process in hardfork_version type
            protocol::hardfork_version next_hardfork; ///< Next planned hardfork version in hardfork_version type (if planned)
            fc::time_point_sec next_hardfork_time; ///< Next planned hardfork version timestamp (if planned)
        };

        typedef multi_index_container<hardfork_property_object, indexed_by<ordered_unique<
                member<hardfork_property_object, hardfork_property_object::id_type, &hardfork_property_object::id>>>,
                allocator<hardfork_property_object> > hardfork_property_index;

    }
} // namespace golos::chain

FC_REFLECT((golos::chain::hardfork_property_object),
           (id)(processed_hardforks)(last_hardfork)(current_hardfork_version)(next_hardfork)(next_hardfork_time))
CHAINBASE_SET_INDEX_TYPE(golos::chain::hardfork_property_object, golos::chain::hardfork_property_index)

#endif //GOLOS_HARDFORK_OBJECT_HPP
