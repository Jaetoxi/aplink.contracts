#include <aplink.token/aplink.token.hpp>
#include <aplink.token/safemath.hpp>
#include <aplink.token/utils.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

static constexpr uint64_t REWARD_PECENT       = 500;
static constexpr uint64_t YEAR_SECONDS        = 365 * 24 * 3600;

namespace aplink {

using namespace eosio;
using namespace wasm;
using namespace wasm::safemath;

void token::create( const name&   issuer,
                    const asset&  maximum_supply )
{
  require_auth( get_self() );

  auto sym = maximum_supply.symbol;
  check( sym.is_valid(), "invalid symbol name" );
  check( maximum_supply.is_valid(), "invalid supply");
  check( maximum_supply.amount > 0, "max-supply must be positive");

  stats statstable( get_self(), sym.code().raw() );
  auto existing = statstable.find( sym.code().raw() );
  check( existing == statstable.end(), "token with symbol already exists" );

  statstable.emplace( get_self(), [&]( auto& s ) {
      s.supply.symbol = maximum_supply.symbol;
      s.max_supply    = maximum_supply;
      s.issuer        = issuer;
  });
}


void token::issue( const name& to, const asset& quantity, const string& memo )
{
  require_auth( to );
  auto sym = quantity.symbol;

  check( sym.is_valid(), "invalid symbol name" );
  check( memo.size() <= 256, "memo has more than 256 bytes" );
  stats statstable( get_self(), sym.code().raw() );
  auto existing = statstable.find( sym.code().raw() );
  check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
  const auto& st = *existing;
  check( to == st.issuer, "can only be executed by issuer account" );

  check( quantity.is_valid(), "invalid quantity" );
  check( quantity.amount > 0, "must issue positive quantity" );

  check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
  check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

  statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply += quantity;
  });

  add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void token::burn(const name&        predator,
                 const symbol&      symbol,
                 const name&        victim)
{
  require_auth( predator );

  accounts acnts( get_self(), victim.value );
  auto acnt_itr = acnts.find( symbol.code().raw() );

  auto quantity = acnt_itr->balance;
  check( acnt_itr->expired_at <  current_time_point(), "premature to burn" );

  check( quantity.is_valid(), "invalid quantity" );
  check( quantity.amount > 0, "must burn positive quantity" );

  uint64_t to_reward = mul( quantity.amount, REWARD_PECENT );
  uint64_t to_burn = quantity.amount - to_reward;
  check( to_reward > 0, "reward quantity must be positive");
  auto reward_quantity  = ASSET( to_reward, quantity.symbol );
  auto burn_quantity    = ASSET( to_burn, quantity.symbol );

  auto sym = quantity.symbol;
  check( sym.is_valid(), "invalid symbol" );

  stats statstable( get_self(), sym.code().raw() );
  auto existing = statstable.find( sym.code().raw() );
  check( existing != statstable.end(), "token with symbol does not exist" );
  const auto& st = *existing;
  check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

  sub_balance( victim, quantity );
  add_balance( predator, reward_quantity, predator);

  require_recipient( victim );
  NOTIFY_REWARD( predator, victim, reward_quantity )

  statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply -= burn_quantity;
  });
}

void token::notifyreward(const name& predator, const name& victim, const asset& reward_quantity) {
    require_auth( _self );

    require_recipient( predator );
}

void token::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{
    check( from != to, "cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    accounts from_acnts( get_self(), from.value );
    auto from_acnt = from_acnts.find( quantity.symbol.code().raw());

    accounts to_acnts( get_self(), to.value );
    auto to_acnt = to_acnts.find( quantity.symbol.code().raw());

    if(from_acnt == from_acnts.end() || !from_acnt->allow_send) {
       check( to_acnt != to_acnts.end() && to_acnt->allow_recv, "no permistion for transfer" );
    }

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::sub_balance( const name& owner, const asset& value ) {
  accounts from_acnts( get_self(), owner.value );

  const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
  check( from.balance.amount >= value.amount, "overdrawn balance" );

  from_acnts.modify( from, owner, [&]( auto& a ) {
    a.balance -= value;
    if (a.balance.amount != 0) {
      a.expired_at = current_time_point() + seconds(YEAR_SECONDS);
    }
  });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
  accounts to_acnts( get_self(), owner.value );
  auto to = to_acnts.find( value.symbol.code().raw() );
  if( to == to_acnts.end() ) {
    to_acnts.emplace( ram_payer, [&]( auto& a ){
      a.balance = value;
      a.sum_balance = value;
      a.expired_at = current_time_point() + seconds(YEAR_SECONDS);
    });
  } else {
    to_acnts.modify( to, same_payer, [&]( auto& a ) {
      a.balance += value;
      a.sum_balance += value;
      a.expired_at = current_time_point() + seconds(YEAR_SECONDS);
    });
  }
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
  require_auth( ram_payer );
  check( is_account( owner ), "owner account does not exist" );

  auto sym_code_raw = symbol.code().raw();
  stats statstable( get_self(), sym_code_raw );
  const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
  check( st.supply.symbol == symbol, "symbol precision mismatch" );

  accounts acnts( get_self(), owner.value );
  auto it = acnts.find( sym_code_raw );
  if( it == acnts.end() ) {
    acnts.emplace( ram_payer, [&]( auto& a ){
      a.balance = asset{0, symbol};
      a.sum_balance = asset{0, symbol};
    });
  }
}

// void token::close( const name& owner, const symbol& symbol )
// {
//    require_auth( owner );
//    accounts acnts( get_self(), owner.value );
//    auto it = acnts.find( symbol.code().raw() );
//    check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
//    check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
//    acnts.erase( it );
// }

void token::setacctperms(const name& issuer, const name& to, const symbol& symbol,  const bool& allowsend, const bool& allowrecv) {
    require_auth( issuer );
    require_issuer(issuer, symbol);

    check( is_account( to ), "to account does not exist");
    check(symbol == APL_SYMBOL, "invalid APL symbol");

    accounts acnts( get_self(), to.value );
    auto it = acnts.find( symbol.code().raw() );
    if( it == acnts.end() ) {
      acnts.emplace( issuer, [&]( auto& a ){
        a.balance = asset(0, APL_SYMBOL);
        a.allow_send = allowsend;
        a.allow_recv = allowrecv;
        a.sum_balance = asset(0, APL_SYMBOL);
        a.expired_at = current_time_point() + seconds(YEAR_SECONDS);
      });
   } else {
      acnts.modify( it, issuer, [&]( auto& a ) {
        a.allow_send = allowsend;
        a.allow_recv = allowrecv;
        a.expired_at = current_time_point() + seconds(YEAR_SECONDS);
      });
   }

}

} /// namespace eosio
