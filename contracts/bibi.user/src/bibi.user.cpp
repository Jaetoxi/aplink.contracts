#include <user/bibi.user.hpp>

void bibiuser::create(const name& owner,const string& pubkey)
{
    require_auth( owner );

    chatuser chatuser_t(owner);
    CHECK( !_db.get(chatuser_t), "the user is registered");

    chatuser_t.owner=owner;
    chatuser_t.status=0;
    chatuser_t.pubkey=pubkey;
    chatuser_t.enable=true;
    chatuser_t.vip_ex_time=time_point(); 

    _db.set(chatuser_t);
}

void bibiuser::update(const name& owner,const string& pubkey, const string& nickname, const uint16_t& status, const string& portrait)
{
    require_auth( owner );

    chatuser chatuser_t(owner);
    CHECK( _db.get(chatuser_t), "the user is not registered");
    CHECK(chatuser_t.enable == true,"the user has been disabled");

    if (chatuser_t.vip_ex_time >= current_time_point()){    
        chatuser_t.nickname = nickname;
        chatuser_t.portrait = portrait;
        chatuser_t.pubkey = pubkey;
        chatuser_t.status = status;
    }else{
        CHECK(nickname.empty(), "the user is not top-up and cannot change the name");
        CHECK(portrait.empty(), "the user is not top-up and cannot change the portrait");
        chatuser_t.pubkey = pubkey;
        chatuser_t.status = status;
    }

    _db.set(chatuser_t);

}

void bibiuser::onpayfee(name from, name to, asset quantity, string memo)
{
    if (_self == from ){
        return;
    }

    if (to != _self){
        return;
    }

    CHECK( _gstate.enable, "top-up disable" );

    CHECK( _gstate.fee.amount == quantity.amount, "quantity mismatch" );

    CHECK( _gstate.fee.symbol == quantity.symbol, "symbol mismatch" );
    
    chatuser chatuser_t(from);
    CHECK( _db.get(chatuser_t), "the user is not registered");
    
    if(chatuser_t.vip_ex_time >= current_time_point()){

       chatuser_t.vip_ex_time += eosio::days(_gstate.effective_days);
    }else{

       chatuser_t.vip_ex_time = current_time_point()+eosio::days(_gstate.effective_days);
    }

    _db.set(chatuser_t);
}

void bibiuser::destory(const name& owner)
{
    CHECK( has_auth(_self) || has_auth(owner), "no permistion for destory");

    chatuser chatuser_t(owner);
    CHECK( _db.get(chatuser_t), "the user is not registered");
    CHECK(chatuser_t.enable == true,"the user has been disabled");

    _db.del(chatuser_t);
}

void bibiuser::setconf(const bool& enable, const asset& fee, uint16_t days)
{
    require_auth( _self );
    
    CHECK( fee.is_valid(), "invalid quantity");
    CHECK( fee.amount > 0, "fee must be positive");

    _gstate.fee = fee;
    _gstate.enable = enable;
    _gstate.effective_days = days;

}

void bibiuser::enableuser(const name& owner, const bool& enable)
{
    require_auth( _self );

    chatuser chatuser_t(owner);
    CHECK( _db.get(chatuser_t), "the user is not registered");

    chatuser_t.enable = enable;
    _db.set(chatuser_t);

}