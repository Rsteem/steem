#include <steemit/protocol/authority.hpp>

#include <steemit/application/impacted.hpp>

namespace steemit {
    namespace application {

        using namespace fc;
        using namespace steemit::protocol;

// TODO:  Review all of these, especially no-ops
        struct get_impacted_account_visitor {
            flat_set<account_name_type> &_impacted;

            get_impacted_account_visitor(flat_set<account_name_type> &impact)
                    : _impacted(impact) {
            }

            typedef void result_type;

            template<typename T>
            void operator()(const T &op) {
                op.get_required_posting_authorities(_impacted);
                op.get_required_active_authorities(_impacted);
                op.get_required_owner_authorities(_impacted);
            }

            void operator()(const account_create_operation &op) {
                _impacted.insert(op.new_account_name);
                _impacted.insert(op.creator);
            }

            void operator()(const account_update_operation &op) {
                _impacted.insert(op.account);
            }

            void operator()(const comment_operation &op) {
                _impacted.insert(op.author);
                if (op.parent_author.size()) {
                    _impacted.insert(op.parent_author);
                }
            }

            void operator()(const delete_comment_operation &op) {
                _impacted.insert(op.author);
            }

            void operator()(const vote_operation &op) {
                _impacted.insert(op.voter);
                _impacted.insert(op.author);
            }

            void operator()(const author_reward_operation &op) {
                _impacted.insert(op.author);
            }

            void operator()(const curation_reward_operation &op) {
                _impacted.insert(op.curator);
            }

            void operator()(const liquidity_reward_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const interest_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const fill_convert_request_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const transfer_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
            }

            void operator()(const transfer_to_vesting_operation &op) {
                _impacted.insert(op.from);

                if (op.to != account_name_type() && op.to != op.from) {
                    _impacted.insert(op.to);
                }
            }

            void operator()(const withdraw_vesting_operation &op) {
                _impacted.insert(op.account);
            }

            void operator()(const witness_update_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const account_witness_vote_operation &op) {
                _impacted.insert(op.account);
                _impacted.insert(op.witness);
            }

            void operator()(const account_witness_proxy_operation &op) {
                _impacted.insert(op.account);
                _impacted.insert(op.proxy);
            }

            void operator()(const feed_publish_operation &op) {
                _impacted.insert(op.publisher);
            }

            void operator()(const limit_order_create_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const fill_order_operation &op) {
                _impacted.insert(op.current_owner);
                _impacted.insert(op.open_owner);
            }

            void operator()(const fill_call_order_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const fill_settlement_order_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const limit_order_cancel_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const pow_operation &op) {
                _impacted.insert(op.worker_account);
            }

            void operator()(const fill_vesting_withdraw_operation &op) {
                _impacted.insert(op.from_account);
                _impacted.insert(op.to_account);
            }

            void operator()(const shutdown_witness_operation &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const custom_operation &op) {
                for (auto s: op.required_auths) {
                    _impacted.insert(s);
                }
            }

            void operator()(const request_account_recovery_operation &op) {
                _impacted.insert(op.account_to_recover);
            }

            void operator()(const recover_account_operation &op) {
                _impacted.insert(op.account_to_recover);
            }

            void operator()(const change_recovery_account_operation &op) {
                _impacted.insert(op.account_to_recover);
            }

            void operator()(const escrow_transfer_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            void operator()(const escrow_approve_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            void operator()(const escrow_dispute_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            void operator()(const escrow_release_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            void operator()(const transfer_to_savings_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
            }

            void operator()(const transfer_from_savings_operation &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
            }

            void operator()(const cancel_transfer_from_savings_operation &op) {
                _impacted.insert(op.from);
            }

            void operator()(const decline_voting_rights_operation &op) {
                _impacted.insert(op.account);
            }

            void operator()(const comment_benefactor_reward_operation &op) {
                _impacted.insert(op.benefactor);
                _impacted.insert(op.author);
            }

            void operator()(const delegate_vesting_shares_operation &op) {
                _impacted.insert(op.delegator);
                _impacted.insert(op.delegatee);
            }

            void operator()(const return_vesting_delegation_operation &op) {
                _impacted.insert(op.account);
            }

            void operator()(const asset_update_operation &op) {
                if (op.new_issuer) {
                    _impacted.insert(*(op.new_issuer));
                }
            }

            void operator()(const asset_issue_operation &op) {
                _impacted.insert(op.issue_to_account);
            }

            void operator()(const override_transfer_operation &op) {
                _impacted.insert(op.to);
                _impacted.insert(op.from);
                _impacted.insert(op.issuer);
            }

            //void operator()( const operation& op ){}
        };

        void operation_get_impacted_accounts(const operation &op, flat_set<account_name_type> &result) {
            get_impacted_account_visitor vtor = get_impacted_account_visitor(result);
            op.visit(vtor);
        }

        void transaction_get_impacted_accounts(const transaction &tx, flat_set<account_name_type> &result) {
            for (const auto &op : tx.operations) {
                operation_get_impacted_accounts(op, result);
            }
        }

    }
}
