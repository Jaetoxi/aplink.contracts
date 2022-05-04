#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <wasm_db.hpp>
#include "apollo.token/apollo.token.db.hpp"

namespace apollo {

using std::string;
using namespace eosio;
using namespace wasm::db;

/**
 * The `apollo.token` is NFT-1155 token contract
 * 
 */
class [[eosio::contract("apollo.token")]] token : public contract {
private:
   dbc                 _db;
   global_singleton    _global;
   global_t            _gstate;

public:
   using contract::contract;

   token(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        _db(_self), contract(receiver, code, ds), _global(_self, _self.value) {
        if (_global.exists()) {
            _gstate = _global.get();

        } else { // first init
            _gstate = global_t{};
            _gstate.admin = _self;
        }
    }

    ~token() { _global.set( _gstate, get_self() ); }
   
   ACTION init();
   /*
    * Create asset token
    *
    */
   ACTION create( const name& issuer, const uint16_t& asset_type, const string& uri, const int64_t& maximum_supply );

   /**
    *  This action issues to `to` account a `quantity` of tokens.
    *
    * @param to - the account to issue tokens to, it must be the same as the issuer,
    * @param quntity - the amount of tokens to be issued,
    * @memo - the memo string that accompanies the token issue transaction.
    */
   [[eosio::action]]
   void issue( const name& to, const token_asset& quantity, const string& memo );

   [[eosio::action]]
   void retire( const token_asset& quantity, const string& memo );

   /*
    * Transfers assets.
    *
    * This action transfers one or more assets by changing scope.
    * Sender's RAM will be charged to transfer asset.
    * Transfer will fail if asset is offered for claim or is delegated.
    *
    * @param from is account who sends the asset.
    * @param to is account of receiver.
    * @param asset is array of asset_id & asset_quantity to transfer.
    * @param memo is transfers comment.
    * @return no return value.
    */
   [[eosio::action]]
   ACTION transfer(const name& from, const name& to, const token_asset& quantity, const string& memo );
   using transfer_action = action_wrapper< "transfer"_n, &token::transfer >;

   ACTION setpowasset( const name& issuer, const uint64_t symbid, const pow_asset_meta& asset_meta);

   private:
   void add_balance( const name& owner, const token_asset& value );
   void sub_balance( const name& owner, const token_asset& value );
};
} //namespace apollo
