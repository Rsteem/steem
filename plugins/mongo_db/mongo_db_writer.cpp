#include <golos/plugins/mongo_db/mongo_db_writer.hpp>
#include <golos/plugins/mongo_db/mongo_db_operations.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/protocol/operations.hpp>

#include <fc/log/logger.hpp>
#include <appbase/application.hpp>

#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/array/element.hpp>
#include <bsoncxx/builder/stream/array.hpp>


namespace golos {
namespace plugins {
namespace mongo_db {

    using namespace bsoncxx::types;
    using namespace bsoncxx::builder;
    using bsoncxx::builder::stream::array;
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;

    mongo_db_writer::mongo_db_writer() :
            _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    mongo_db_writer::~mongo_db_writer() {
    }

    bool mongo_db_writer::initialize(const std::string& uri_str, const bool write_raw, const std::vector<std::string>& ops) {
        try {
            uri = mongocxx::uri {uri_str};
            mongo_conn = mongocxx::client {uri};
            db_name = uri.database().empty() ? "Golos" : uri.database();
            mongo_database = mongo_conn[db_name];
            bulk_opts.ordered(false);
            write_raw_blocks = write_raw;

            for (auto& op : ops) {
                if (!op.empty()) {
                    write_operations.insert(op);
                }
            }

            ilog("MongoDB writer initialized.");

            return true;
        }
        catch (mongocxx::exception & ex) {
            wlog("Exception in MongoDB initialize: ${p}", ("p", ex.what()));
            return false;
        }
        catch (...) {
            wlog("Unknown exception in MongoDB writer");
            return false;
        }
    }

    void mongo_db_writer::on_block(const signed_block& block) {

        try {

            //ilog("MongoDB mongo_db_writer::on_block processed blocks: ${e}", ("e", processed_blocks));

            blocks[block.block_num()] = block;

            // Update last irreversible block number
            last_irreversible_block_num = _db.last_non_undoable_block_num();
            if (last_irreversible_block_num >= blocks.begin()->first) {

                // Write all the blocks that has num less then last irreversible block
                while (!blocks.empty() && blocks.begin()->first <= last_irreversible_block_num) {
                    auto head_iter = blocks.begin();

                    try {
                        if (write_raw_blocks) {
                            write_raw_block(head_iter->second, virtual_ops[head_iter->first]);
                        }

                        state_writer st_writer(block);
                        
                        std::unordered_map<std::tuple<std::string, std::string, std::string>,
                            named_document_ptr, string_tuple_hasher> all_docs;

                        // Collecting all operations of block to single map and vector

                        for (const auto& tran : head_iter->second.transactions) {
                            for (const auto& op : tran.operations) {
                                auto result = op.visit(st_writer);
                                for (auto it = result.begin(); it != result.end(); ++it) {
                                    all_docs[it->first] = std::move(it->second);
                                }
                            }
                        }

                        // Writing all to mongo

                        for (auto it = all_docs.begin(); it != all_docs.end(); ++it) {
                            if (!it->second->is_removal) {
                                write_document(it->second);
                            } else {
                                std::string collection_name, key_name, key_value;
                                std::tie(collection_name, key_name, key_value) = it->first;
                                remove_document(key_name, key_value, it->second);
                            }
                        }
                        
                        write_block_operations(st_writer, head_iter->second, virtual_ops[head_iter->first]);
                    }
                    catch (...) {
                        // If some block causes any problems lets remove it from buffer and move on
                        blocks.erase(head_iter);
                        virtual_ops.erase(head_iter->first);
                        throw;
                    }
                    blocks.erase(head_iter);
                    virtual_ops.erase(head_iter->first);
                }
                write_data();
            }

            ++processed_blocks;
        }
        catch (const std::exception& e) {
            wlog("Unknown exception in MongoDB ${e}", ("e", e.what()));
        }
    }

    void mongo_db_writer::on_operation(const golos::chain::operation_notification& note) {
        virtual_ops[note.block].push_back(note.op);
        // remove ops if there were forks and rollbacks
        auto itr = virtual_ops.find(note.block);
        ++itr;
        virtual_ops.erase(itr, virtual_ops.end());
    }

    void mongo_db_writer::write_raw_block(const signed_block& block, const operations& ops) {

        //ilog("mongo_db_writer::write_raw_block ${e}", ("e", block.block_num()));

        operation_writer op_writer;
        document block_doc;
        format_block_info(block, block_doc);

        array transactions_array;

        // Now write every transaction from Block
        for (const auto& tran : block.transactions) {

            document tran_doc;
            format_transaction_info(tran, tran_doc);

            if (!tran.operations.empty()) {

                array operations_array;

                // Write every operation in transaction
                for (const auto& op : tran.operations) {

                    try {
                        operations_array << op.visit(op_writer);
                    }
//                    catch (fc::exception& ex) {
//                        wlog("MongoDB write_raw_block fc::exception ${e}", ("e", ex.what()));
//                    }
//                    catch (mongocxx::exception& ex) {
//                        wlog("Mongodb write_raw_block Mongo exception ${e}", ("e", ex.what()));
//                    }
                    catch (std::exception& ex) {
                        wlog("Mongodb write_raw_block std exception ${e}", ("e", ex.what()));
                    } catch (...) {
                        wlog("Mongodb write_raw_block unknown exception ");
                    }
                }

                static const std::string operations = "operations";
                tran_doc << operations << operations_array;
            }

            transactions_array << tran_doc;
        }

        if (!ops.empty()) {
            array operations_array;
            for (auto &op: ops) {
                try {
                    operations_array << op.visit(op_writer);
                } catch (...) {
                    //
                }
            }
            static const std::string operations = "virtual_operations";
            block_doc << operations << operations_array;
        }

        static const std::string transactions = "transactions";
        block_doc << transactions << transactions_array;

        static const std::string blocks = "blocks";
        if (formatted_blocks.find(blocks) == formatted_blocks.end()) {
            formatted_blocks[blocks] = std::make_unique<mongocxx::bulk_write>(bulk_opts);
        }

        mongocxx::model::insert_one insert_msg{block_doc.view()};
        formatted_blocks[blocks]->append(insert_msg);
    }

    void mongo_db_writer::write_document(const named_document_ptr& named_doc) {
        auto mongo_doc = static_cast<mongo_document*>(named_doc.get());
        if (formatted_blocks.find(named_doc->collection_name) == formatted_blocks.end()) {
            formatted_blocks[named_doc->collection_name] = std::make_unique<mongocxx::bulk_write>(bulk_opts);
        }

        auto view = mongo_doc->doc.view();
        auto itr = view.find("_id");
        if (view.end() == itr) {
            mongocxx::model::insert_one msg{std::move(view)};
            formatted_blocks[named_doc->collection_name]->append(msg);
        } else {
            document filter;
            filter << "_id" << view["_id"].get_oid();

            mongocxx::model::replace_one msg{filter.view(), std::move(view)};
            msg.upsert(true);

            if (!named_doc->index_to_create.empty()) {
                if (indexes.find(named_doc->collection_name) == indexes.end()) {
                    mongo_database[named_doc->collection_name].create_index(document{} << named_doc->index_to_create << 1 << finalize);
                    indexes[named_doc->collection_name] = named_doc->index_to_create;
                }
            }

            formatted_blocks[mongo_doc->collection_name]->append(msg);
        }
    }

    void mongo_db_writer::remove_document(const std::string& key, const std::string& key_value, const named_document_ptr& named_doc) {
        auto rem_doc = static_cast<removal_document*>(named_doc.get());
        auto oid_hash = fc::sha1::hash(key_value).str().substr(0, 24);
        mongo_database[rem_doc->collection_name].update_many(
            document{} << key << bsoncxx::oid(oid_hash) << finalize,
            document{} << "$set" << open_document << "removed" << true << close_document << finalize);
        // Or, to remove permanently:
        //mongo_database[rem_doc->collection_name].delete_many(
        //    document{} << "_id" << bsoncxx::oid(oid_hash) << finalize);
    }

    void mongo_db_writer::write_block_operations(state_writer st_writer, const signed_block& block, const operations& ops) {
        //ilog("mongo_db_writer::write_block_operations ${e}", ("e", block.block_num()));

        for (auto& op: ops) {
            auto result = op.visit(st_writer);

            for (auto it = result.begin(); it != result.end(); ++it) {
                if (!it->second->is_removal) {
                    write_document(it->second);
                } else {
                    std::string collection_name, key_name, key_value;
                    std::tie(collection_name, key_name, key_value) = it->first;
                    remove_document(key_name, key_value, it->second);
                }
            }
        }
    }

    void mongo_db_writer::format_block_info(const signed_block& block, document& doc) {
        doc << "block_num"              << static_cast<int32_t>(block.block_num())
            << "block_id"               << block.id().str()
            << "block_prev_block_id"    << block.previous.str()
            << "block_timestamp"        << block.timestamp
            << "block_witness"          << block.witness
            << "block_created_at"       << fc::time_point::now();
    }

    void mongo_db_writer::format_transaction_info(const signed_transaction& tran, document& doc) {
        doc << "transaction_id"             << tran.id().str()
            << "transaction_ref_block_num"  << static_cast<int32_t>(tran.ref_block_num)
            << "transaction_expiration"     << tran.expiration;
    }

    void mongo_db_writer::write_data() {
        auto iter = formatted_blocks.begin();
        for (; iter != formatted_blocks.end(); ++iter) {

            auto& oper = *iter;
            try {
                const std::string& collection_name = oper.first;
                mongocxx::collection _collection = mongo_database[collection_name];

                auto& bulkp = oper.second;
                if (!_collection.bulk_write(*bulkp)) {
                    wlog("Failed to write blocks to Mongo DB");
                }
            }
            catch (const std::exception& e) {
                wlog("Unknown exception while writing blocks to mongo: ${e}", ("e", e.what()));
                // If we got some errors writing block into mongo just skip this block and move on
                formatted_blocks.erase(iter);
                throw;
            }
        }
        formatted_blocks.clear();
    }
}}}
