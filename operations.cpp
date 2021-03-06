/*
 * operations.cpp
 *
 *  Created on: 2010-9-18
 *      Author: Argon
 */
#include "field.h"
#include "duel.h"
#include "card.h"
#include "effect.h"
#include "group.h"
#include "interpreter.h"
#include <algorithm>

int32 field::negate_chain(uint8 chaincount) {
	if(core.current_chain.size() == 0)
		return FALSE;
	if(chaincount > core.current_chain.size() || chaincount < 1)
		chaincount = core.current_chain.size();
	chain& pchain = core.current_chain[chaincount - 1];
	if(!(pchain.flag & CHAIN_DISABLE_ACTIVATE) && is_chain_negatable(pchain.chain_count)
	        && pchain.triggering_effect->handler->is_affect_by_effect(core.reason_effect) ) {
		pchain.flag |= CHAIN_DISABLE_ACTIVATE;
		pchain.disable_reason = core.reason_effect;
		pchain.disable_player = core.reason_player;
		if((pchain.triggering_effect->type & EFFECT_TYPE_ACTIVATE) && (pchain.triggering_effect->handler->current.location == LOCATION_SZONE)) {
			pchain.triggering_effect->handler->set_status(STATUS_LEAVE_CONFIRMED, TRUE);
			pchain.triggering_effect->handler->set_status(STATUS_ACTIVATE_DISABLED, TRUE);
		}
		pduel->write_buffer8(MSG_CHAIN_NEGATED);
		pduel->write_buffer8(chaincount);
		if(pchain.triggering_effect->is_flag(EFFECT_FLAG2_NAGA))
			return FALSE;
		return TRUE;
	}
	return FALSE;
}
int32 field::disable_chain(uint8 chaincount) {
	if(core.current_chain.size() == 0)
		return FALSE;
	if(chaincount > core.current_chain.size() || chaincount < 1)
		chaincount = core.current_chain.size();
	chain& pchain = core.current_chain[chaincount - 1];
	if(!(pchain.flag & CHAIN_DISABLE_EFFECT) && is_chain_disablable(pchain.chain_count)
	        && pchain.triggering_effect->handler->is_affect_by_effect(core.reason_effect)) {
		core.current_chain[chaincount - 1].flag |= CHAIN_DISABLE_EFFECT;
		core.current_chain[chaincount - 1].disable_reason = core.reason_effect;
		core.current_chain[chaincount - 1].disable_player = core.reason_player;
		pduel->write_buffer8(MSG_CHAIN_DISABLED);
		pduel->write_buffer8(chaincount);
		if(pchain.triggering_effect->is_flag(EFFECT_FLAG2_NAGA))
			return FALSE;
		return TRUE;
	}
	return FALSE;
}
void field::change_chain_effect(uint8 chaincount, int32 rep_op) {
	if(core.current_chain.size() == 0)
		return;
	if(chaincount > core.current_chain.size() || chaincount < 1)
		chaincount = core.current_chain.size();
	core.current_chain[chaincount - 1].replace_op = rep_op;
}
void field::change_target(uint8 chaincount, group* targets) {
	if(core.current_chain.size() == 0)
		return;
	if(chaincount > core.current_chain.size() || chaincount < 1)
		chaincount = core.current_chain.size();
	group* ot = core.current_chain[chaincount - 1].target_cards;
	if(ot) {
		effect* te = core.current_chain[chaincount - 1].triggering_effect;
		for(auto cit = ot->container.begin(); cit != ot->container.end(); ++cit)
			(*cit)->release_relation(core.current_chain[chaincount - 1]);
		ot->container = targets->container;
		for(auto cit = ot->container.begin(); cit != ot->container.end(); ++cit)
			(*cit)->create_relation(core.current_chain[chaincount - 1]);
		if(te->is_flag(EFFECT_FLAG_CARD_TARGET)) {
			for(auto cit = ot->container.begin(); cit != ot->container.end(); ++cit) {
				if((*cit)->current.location & 0x30)
					move_card((*cit)->current.controler, (*cit), (*cit)->current.location, 0);
				pduel->write_buffer8(MSG_BECOME_TARGET);
				pduel->write_buffer8(1);
				pduel->write_buffer32((*cit)->get_info_location());
			}
		}
	}
}
void field::change_target_player(uint8 chaincount, uint8 playerid) {
	if(core.current_chain.size() == 0)
		return;
	if(chaincount > core.current_chain.size() || chaincount < 1)
		chaincount = core.current_chain.size();
	core.current_chain[chaincount - 1].target_player = playerid;
}
void field::change_target_param(uint8 chaincount, int32 param) {
	if(core.current_chain.size() == 0)
		return;
	if(chaincount > core.current_chain.size() || chaincount < 1)
		chaincount = core.current_chain.size();
	core.current_chain[chaincount - 1].target_param = param;
}
void field::remove_counter(uint32 reason, card* pcard, uint32 rplayer, uint32 s, uint32 o, uint32 countertype, uint32 count) {
	add_process(PROCESSOR_REMOVE_COUNTER, 0, (effect*)(size_t) reason, (group*)pcard, (rplayer << 16) + (s << 8) + o, countertype + (count << 16));
}
void field::remove_overlay_card(uint32 reason, card* pcard, uint32 rplayer, uint32 s, uint32 o, uint16 min, uint16 max) {
	add_process(PROCESSOR_REMOVEOL_S, 0, (effect*)(size_t) reason, (group*)pcard, (rplayer << 16) + (s << 8) + o, (max << 16) + min);
}
void field::get_control(card_set* targets, effect* reason_effect, uint32 reason_player, uint32 playerid, uint32 reset_phase, uint32 reset_count) {
	group* ng = pduel->new_group(*targets);
	ng->is_readonly = TRUE;
	add_process(PROCESSOR_GET_CONTROL, 0, reason_effect, ng, 0, (reason_player << 28) + (playerid << 24) + (reset_phase << 8) + reset_count);
}
void field::get_control(card* target, effect* reason_effect, uint32 reason_player, uint32 playerid, uint32 reset_phase, uint32 reset_count) {
	card_set tset;
	tset.insert(target);
	get_control(&tset, reason_effect, reason_player, playerid, reset_phase, reset_count);
}
void field::swap_control(effect* reason_effect, uint32 reason_player, card* pcard1, card* pcard2, uint32 reset_phase, uint32 reset_count) {
	add_process(PROCESSOR_SWAP_CONTROL, 0, reason_effect, (group*)pcard1, (ptr)pcard2, (reason_player << 28) + (reset_phase << 8) + reset_count);
}
void field::equip(uint32 equip_player, card* equip_card, card* target, uint32 up, uint32 is_step) {
	add_process(PROCESSOR_EQUIP, 0, 0, (group*)target, (ptr)equip_card, equip_player + (up << 16) + (is_step << 24));
}
void field::draw(effect* reason_effect, uint32 reason, uint32 reason_player, uint32 playerid, uint32 count) {
	add_process(PROCESSOR_DRAW, 0, reason_effect, 0, reason, (reason_player << 28) + (playerid << 24) + (count & 0xffffff));
}
void field::damage(effect* reason_effect, uint32 reason, uint32 reason_player, card* reason_card, uint32 playerid, uint32 amount, uint32 is_step) {
	uint32 arg2 = (is_step << 28) + (reason_player << 26) + (playerid << 24) + (amount & 0xffffff);
	if(reason & REASON_BATTLE)
		add_process(PROCESSOR_DAMAGE, 0, (effect*)reason_card, 0, reason, arg2);
	else
		add_process(PROCESSOR_DAMAGE, 0, reason_effect, 0, reason, arg2);
}
void field::recover(effect* reason_effect, uint32 reason, uint32 reason_player, uint32 playerid, uint32 amount, uint32 is_step) {
	add_process(PROCESSOR_RECOVER, 0, reason_effect, 0, reason, (is_step << 28) + (reason_player << 26) + (playerid << 24) + (amount & 0xffffff));
}
void field::summon(uint32 sumplayer, card* target, effect* proc, uint32 ignore_count, uint32 min_tribute) {
	add_process(PROCESSOR_SUMMON_RULE, 0, proc, (group*)target, sumplayer + (ignore_count << 8) + (min_tribute << 16), 0);
}
void field::special_summon_rule(uint32 sumplayer, card* target, uint32 summon_type) {
	add_process(PROCESSOR_SPSUMMON_RULE, 0, 0, (group*)target, sumplayer, summon_type);
}
void field::special_summon(card_set* target, uint32 sumtype, uint32 sumplayer, uint32 playerid, uint32 nocheck, uint32 nolimit, uint32 positions) {
	if((positions & POS_FACEDOWN) && is_player_affected_by_effect(sumplayer, EFFECT_DEVINE_LIGHT))
		positions = (positions & POS_FACEUP) | (positions >> 1);
	for(auto cit = target->begin(); cit != target->end(); ++cit) {
		card* pcard = *cit;
		pcard->temp.reason = pcard->current.reason;
		pcard->temp.reason_effect = pcard->current.reason_effect;
		pcard->temp.reason_player = pcard->current.reason_player;
		pcard->summon_info = (sumtype & 0xf00ffff) | SUMMON_TYPE_SPECIAL | ((uint32)pcard->current.location << 16);
		pcard->summon_player = sumplayer;
		pcard->current.reason = REASON_SPSUMMON;
		pcard->current.reason_effect = core.reason_effect;
		pcard->current.reason_player = core.reason_player;
		pcard->operation_param = (playerid << 24) + (nocheck << 16) + (nolimit << 8) + positions;
	}
	group* pgroup = pduel->new_group(*target);
	pgroup->is_readonly = TRUE;
	add_process(PROCESSOR_SPSUMMON, 0, core.reason_effect, pgroup, core.reason_player, 0);
}
void field::special_summon_step(card* target, uint32 sumtype, uint32 sumplayer, uint32 playerid, uint32 nocheck, uint32 nolimit, uint32 positions) {
	if((positions & POS_FACEDOWN) && is_player_affected_by_effect(sumplayer, EFFECT_DEVINE_LIGHT))
		positions = (positions & POS_FACEUP) | (positions >> 1);
	target->temp.reason = target->current.reason;
	target->temp.reason_effect = target->current.reason_effect;
	target->temp.reason_player = target->current.reason_player;
	target->summon_info = (sumtype & 0xf00ffff) | SUMMON_TYPE_SPECIAL | ((uint32)target->current.location << 16);
	target->summon_player = sumplayer;
	target->current.reason = REASON_SPSUMMON;
	target->current.reason_effect = core.reason_effect;
	target->current.reason_player = core.reason_player;
	target->operation_param = (playerid << 24) + (nocheck << 16) + (nolimit << 8) + positions;
	add_process(PROCESSOR_SPSUMMON_STEP, 0, core.reason_effect, 0, 0, (ptr)target);
}
void field::special_summon_complete(effect* reason_effect, uint8 reason_player) {
	if(core.special_summoning.size() == 0)
		return;
	group* ng = pduel->new_group();
	ng->container.swap(core.special_summoning);
	ng->is_readonly = TRUE;
	//core.special_summoning.clear();
	add_process(PROCESSOR_SPSUMMON_COMP_S, 0, reason_effect, ng, reason_player, 0);
}
// destroy: 1st arg is card or card_set=>card version, otherwise=>step version
// destroy in script: here->PROCESSOR_DESTROY, step 0
// set current.reason
void field::destroy(card_set* targets, effect* reason_effect, uint32 reason, uint32 reason_player, uint32 playerid, uint32 destination, uint32 sequence) {
	uint32 p;
	for(auto cit = targets->begin(); cit != targets->end();) {
		card* pcard = *cit;
		if(pcard->is_status(STATUS_DESTROY_CONFIRMED)) {
			targets->erase(cit++);
			continue;
		}
		pcard->temp.reason = pcard->current.reason;
		pcard->current.reason = reason;
		if(reason_player != 5) {
			pcard->temp.reason_effect = pcard->current.reason_effect;
			pcard->temp.reason_player = pcard->current.reason_player;
			if(reason_effect)
				pcard->current.reason_effect = reason_effect;
			pcard->current.reason_player = reason_player;
		}
		p = playerid;
		if(!(destination & (LOCATION_HAND + LOCATION_DECK + LOCATION_REMOVED)))
			destination = LOCATION_GRAVE;
		if(destination && p == PLAYER_NONE)
			p = pcard->owner;
		if(destination & (LOCATION_GRAVE + LOCATION_REMOVED))
			p = pcard->owner;
		pcard->set_status(STATUS_DESTROY_CONFIRMED, TRUE);
		pcard->operation_param = (POS_FACEUP << 24) + (p << 16) + (destination << 8) + sequence;
		++cit;
	}
	group* ng = pduel->new_group(*targets);
	ng->is_readonly = TRUE;
	add_process(PROCESSOR_DESTROY, 0, reason_effect, ng, reason, reason_player);
}
void field::destroy(card* target, effect* reason_effect, uint32 reason, uint32 reason_player, uint32 playerid, uint32 destination, uint32 sequence) {
	card_set tset;
	tset.insert(target);
	destroy(&tset, reason_effect, reason, reason_player, playerid, destination, sequence);
}
void field::release(card_set* targets, effect* reason_effect, uint32 reason, uint32 reason_player) {
	for(auto cit = targets->begin(); cit != targets->end(); ++cit) {
		card* pcard = *cit;
		pcard->temp.reason = pcard->current.reason;
		pcard->temp.reason_effect = pcard->current.reason_effect;
		pcard->temp.reason_player = pcard->current.reason_player;
		pcard->current.reason = reason;
		pcard->current.reason_effect = reason_effect;
		pcard->current.reason_player = reason_player;
		pcard->operation_param = (POS_FACEUP << 24) + ((uint32)(pcard->owner) << 16) + (LOCATION_GRAVE << 8);
	}
	group* ng = pduel->new_group(*targets);
	ng->is_readonly = TRUE;
	add_process(PROCESSOR_RELEASE, 0, reason_effect, ng, reason, reason_player);
}
void field::release(card* target, effect* reason_effect, uint32 reason, uint32 reason_player) {
	card_set tset;
	tset.insert(target);
	release(&tset, reason_effect, reason, reason_player);
}
// set current.reason and send to locations other than LOCATION_ONFIELD
// send-to in scripts: here->PROCESSOR_SENDTO, step 0
void field::send_to(card_set* targets, effect* reason_effect, uint32 reason, uint32 reason_player, uint32 playerid, uint32 destination, uint32 sequence, uint32 position) {
	if(destination & LOCATION_ONFIELD)
		return;
	uint32 p, pos;
	for(auto cit = targets->begin(); cit != targets->end(); ) {
		card* pcard = *cit++;
		if((destination & LOCATION_EXTRA) && !(pcard->data.type & TYPE_PENDULUM)) {
			targets->erase(pcard);
			continue;
		}
		pcard->temp.reason = pcard->current.reason;
		pcard->temp.reason_effect = pcard->current.reason_effect;
		pcard->temp.reason_player = pcard->current.reason_player;
		pcard->current.reason = reason;
		pcard->current.reason_effect = reason_effect;
		pcard->current.reason_player = reason_player;
		p = playerid;
		// send to hand from deck  & playerid not given => send to the hand of controler
		if(p == PLAYER_NONE && (destination & LOCATION_HAND) && (pcard->current.location & LOCATION_DECK) && pcard->current.controler == reason_player)
			p = reason_player;
		if(destination & (LOCATION_GRAVE + LOCATION_REMOVED) || p == PLAYER_NONE)
			p = pcard->owner;
		if(destination != LOCATION_REMOVED)
			pos = POS_FACEUP;
		else if(position == 0)
			pos = pcard->current.position;
		else pos = position;
		pcard->operation_param = (pos << 24) + (p << 16) + (destination << 8) + (sequence);
	}
	group* ng = pduel->new_group(*targets);
	ng->is_readonly = TRUE;
	add_process(PROCESSOR_SENDTO, 0, reason_effect, ng, reason, reason_player);
}
void field::send_to(card* target, effect* reason_effect, uint32 reason, uint32 reason_player, uint32 playerid, uint32 destination, uint32 sequence, uint32 position) {
	card_set tset;
	tset.insert(target);
	send_to(&tset, reason_effect, reason, reason_player, playerid, destination, sequence, position);
}
void field::move_to_field(card* target, uint32 move_player, uint32 playerid, uint32 destination, uint32 positions, uint32 enable, uint32 ret, uint32 is_equip) {
	if(!(destination & (LOCATION_MZONE + LOCATION_SZONE)) || !positions)
		return;
	if(destination == target->current.location && playerid == target->current.controler)
		return;
	target->operation_param = (move_player << 24) + (playerid << 16) + (destination << 8) + positions;
	add_process(PROCESSOR_MOVETOFIELD, 0, 0, (group*)target, enable, ret + (is_equip << 8));
}
void field::change_position(card_set* targets, effect* reason_effect, uint32 reason_player, uint32 au, uint32 ad, uint32 du, uint32 dd, uint32 flag, uint32 enable) {
	group* ng = pduel->new_group(*targets);
	ng->is_readonly = TRUE;
	for(auto cit = targets->begin(); cit != targets->end(); ++cit) {
		card* pcard = *cit;
		if(pcard->current.position == POS_FACEUP_ATTACK) pcard->operation_param = au;
		else if(pcard->current.position == POS_FACEDOWN_DEFENSE) pcard->operation_param = dd;
		else if(pcard->current.position == POS_FACEUP_DEFENSE) pcard->operation_param = du;
		else pcard->operation_param = ad;
		pcard->operation_param |= flag;
	}
	add_process(PROCESSOR_CHANGEPOS, 0, reason_effect, ng, reason_player, enable);
}
void field::change_position(card* target, effect* reason_effect, uint32 reason_player, uint32 npos, uint32 flag, uint32 enable) {
	group* ng = pduel->new_group(target);
	ng->is_readonly = TRUE;
	target->operation_param = npos;
	target->operation_param |= flag;
	add_process(PROCESSOR_CHANGEPOS, 0, reason_effect, ng, reason_player, enable);
}
int32 field::draw(uint16 step, effect* reason_effect, uint32 reason, uint8 reason_player, uint8 playerid, uint32 count) {
	switch(step) {
	case 0: {
		card* pcard;
		card_vector cv;
		uint32 drawed = 0;
		uint32 public_count = 0;
		if(!(reason & REASON_RULE) && !is_player_can_draw(playerid)) {
			returns.ivalue[0] = 0;
			return TRUE;
		}
		core.overdraw[playerid] = FALSE;
		for(uint32 i = 0; i < count; ++i) {
			if(player[playerid].list_main.size() == 0) {
				core.overdraw[playerid] = TRUE;
				break;
			}
			drawed++;
			pcard = player[playerid].list_main.back();
			pcard->enable_field_effect(false);
			pcard->cancel_field_effect();
			player[playerid].list_main.pop_back();
			pcard->previous.controler = pcard->current.controler;
			pcard->previous.location = pcard->current.location;
			pcard->previous.sequence = pcard->current.sequence;
			pcard->previous.position = pcard->current.position;
			pcard->current.controler = PLAYER_NONE;
			pcard->current.reason_effect = reason_effect;
			pcard->current.reason_player = reason_player;
			pcard->current.reason = reason | REASON_DRAW;
			pcard->current.location = 0;
			add_card(playerid, pcard, LOCATION_HAND, 0);
			pcard->enable_field_effect(true);
			effect* pub = pcard->is_affected_by_effect(EFFECT_PUBLIC);
			if(pub)
				public_count++;
			pcard->current.position = pub ? POS_FACEUP : POS_FACEDOWN;
			cv.push_back(pcard);
			pcard->reset(RESET_TOHAND, RESET_EVENT);
		}
		core.hint_timing[playerid] |= TIMING_DRAW + TIMING_TOHAND;
		adjust_instant();
		core.units.begin()->arg2 = (core.units.begin()->arg2 & 0xff000000) + drawed;
		card_set* drawed_set = new card_set;
		core.units.begin()->ptarget = (group*)drawed_set;
		drawed_set->insert(cv.begin(), cv.end());
		if(drawed) {
			if(core.global_flag & GLOBALFLAG_DECK_REVERSE_CHECK) {
				if(player[playerid].list_main.size()) {
					card* ptop = player[playerid].list_main.back();
					if(core.deck_reversed || (ptop->current.position == POS_FACEUP_DEFENSE)) {
						pduel->write_buffer8(MSG_DECK_TOP);
						pduel->write_buffer8(playerid);
						pduel->write_buffer8(drawed);
						if(ptop->current.position != POS_FACEUP_DEFENSE)
							pduel->write_buffer32(ptop->data.code);
						else
							pduel->write_buffer32(ptop->data.code | 0x80000000);
					}
				}
			}
			pduel->write_buffer8(MSG_DRAW);
			pduel->write_buffer8(playerid);
			pduel->write_buffer8(drawed);
			for(uint32 i = 0; i < drawed; ++i)
				pduel->write_buffer32(cv[i]->data.code | (cv[i]->is_position(POS_FACEUP) ? 0x80000000 : 0));
			if(core.deck_reversed && (public_count < drawed)) {
				pduel->write_buffer8(MSG_CONFIRM_CARDS);
				pduel->write_buffer8(1 - playerid);
				pduel->write_buffer8(drawed_set->size());
				for(auto cit = drawed_set->begin(); cit != drawed_set->end(); ++cit) {
					pduel->write_buffer32((*cit)->data.code);
					pduel->write_buffer8((*cit)->current.controler);
					pduel->write_buffer8((*cit)->current.location);
					pduel->write_buffer8((*cit)->current.sequence);
				}
				shuffle(playerid, LOCATION_HAND);
			}
			for(auto cit = drawed_set->begin(); cit != drawed_set->end(); ++cit)
				raise_single_event((*cit), 0, EVENT_TO_HAND, reason_effect, reason, reason_player, playerid, 0);
			process_single_event();
			raise_event(drawed_set, EVENT_DRAW, reason_effect, reason, reason_player, playerid, drawed);
			raise_event(drawed_set, EVENT_TO_HAND, reason_effect, reason, reason_player, playerid, drawed);
			process_instant_event();
		}
		return FALSE;
	}
	case 1: {
		card_set* drawed_set = (card_set*)core.units.begin()->ptarget;
		core.operated_set.swap(*drawed_set);
		delete drawed_set;
		returns.ivalue[0] = count;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::damage(uint16 step, effect* reason_effect, uint32 reason, uint8 reason_player, card* reason_card, uint8 playerid, uint32 amount, uint32 is_step) {
	switch(step) {
	case 0: {
		effect_set eset;
		returns.ivalue[0] = amount;
		if(amount <= 0)
			return TRUE;
		if(!(reason & REASON_RDAMAGE)) {
			filter_player_effect(playerid, EFFECT_REVERSE_DAMAGE, &eset);
			for(int32 i = 0; i < eset.size(); ++i) {
				pduel->lua->add_param(reason_effect, PARAM_TYPE_EFFECT);
				pduel->lua->add_param(reason, PARAM_TYPE_INT);
				pduel->lua->add_param(reason_player, PARAM_TYPE_INT);
				pduel->lua->add_param(reason_card, PARAM_TYPE_CARD);
				if(eset[i]->check_value_condition(4)) {
					recover(reason_effect, (reason & REASON_RRECOVER) | REASON_RDAMAGE | REASON_EFFECT, reason_player, playerid, amount, is_step);
					core.units.begin()->step = 2;
					return FALSE;
				}
			}
		}
		uint32 val = amount;
		eset.clear();
		filter_player_effect(playerid, EFFECT_CHANGE_DAMAGE, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			pduel->lua->add_param(reason_effect, PARAM_TYPE_EFFECT);
			pduel->lua->add_param(amount, PARAM_TYPE_INT);
			pduel->lua->add_param(reason, PARAM_TYPE_INT);
			pduel->lua->add_param(reason_player, PARAM_TYPE_INT);
			pduel->lua->add_param(reason_card, PARAM_TYPE_CARD);
			val = eset[i]->get_value(5);
			returns.ivalue[0] = val;
			if(val == 0)
				return TRUE;
		}
		eset.clear();
		filter_player_effect(playerid, EFFECT_REFLECT_DAMAGE, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			pduel->lua->add_param(reason_effect, PARAM_TYPE_EFFECT);
			pduel->lua->add_param(val, PARAM_TYPE_INT);
			pduel->lua->add_param(reason, PARAM_TYPE_INT);
			pduel->lua->add_param(reason_player, PARAM_TYPE_INT);
			pduel->lua->add_param(reason_card, PARAM_TYPE_CARD);
			if(eset[i]->check_value_condition(5)) {
				core.units.begin()->arg2 |= 1 << 29;
				break;
			}
		}
		core.units.begin()->arg2 = (core.units.begin()->arg2 & 0xff000000) | (val & 0xffffff);
		if(is_step) {
			core.units.begin()->step = 9;
			return TRUE;
		}
		return FALSE;
	}
	case 1: {
		uint32 is_reflect = (core.units.begin()->arg2 >> 29) & 1;
		if(is_reflect)
			playerid = 1 - playerid;
		if(is_reflect || (reason & REASON_RRECOVER))
			core.units.begin()->step = 2;
		core.hint_timing[playerid] |= TIMING_DAMAGE;
		player[playerid].lp -= amount;
		pduel->write_buffer8(MSG_DAMAGE);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(amount);
		raise_event(reason_card, EVENT_DAMAGE, reason_effect, reason, reason_player, playerid, amount);
		if(reason == REASON_BATTLE && reason_card) {
			if((player[playerid].lp <= 0) && (core.attack_target == 0) && reason_card->is_affected_by_effect(EFFECT_MATCH_KILL)) {
				pduel->write_buffer8(MSG_MATCH_KILL);
				pduel->write_buffer32(reason_card->data.code);
			}
			raise_single_event(reason_card, 0, EVENT_BATTLE_DAMAGE, 0, 0, reason_player, playerid, amount);
			raise_event(reason_card, EVENT_BATTLE_DAMAGE, 0, 0, reason_player, playerid, amount);
			process_single_event();
		}
		process_instant_event();
		return FALSE;
	}
	case 2: {
		returns.ivalue[0] = amount;
		return TRUE;
	}
	case 3: {
		returns.ivalue[0] = 0;
		return TRUE;
	}
	case 10: {
		//dummy
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::recover(uint16 step, effect* reason_effect, uint32 reason, uint8 reason_player, uint8 playerid, uint32 amount, uint32 is_step) {
	switch(step) {
	case 0: {
		effect_set eset;
		returns.ivalue[0] = amount;
		if(amount <= 0)
			return TRUE;
		if(!(reason & REASON_RRECOVER)) {
			filter_player_effect(playerid, EFFECT_REVERSE_RECOVER, &eset);
			for(int32 i = 0; i < eset.size(); ++i) {
				pduel->lua->add_param(reason_effect, PARAM_TYPE_EFFECT);
				pduel->lua->add_param(reason, PARAM_TYPE_INT);
				pduel->lua->add_param(reason_player, PARAM_TYPE_INT);
				if(eset[i]->check_value_condition(3)) {
					damage(reason_effect, (reason & REASON_RDAMAGE) | REASON_RRECOVER | REASON_EFFECT, reason_player, 0, playerid, amount, is_step);
					core.units.begin()->step = 2;
					return FALSE;
				}
			}
		}
		if(is_step) {
			core.units.begin()->step = 9;
			return TRUE;
		}
		return FALSE;
	}
	case 1: {
		if(reason & REASON_RDAMAGE)
			core.units.begin()->step = 2;
		core.hint_timing[playerid] |= TIMING_RECOVER;
		player[playerid].lp += amount;
		pduel->write_buffer8(MSG_RECOVER);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(amount);
		raise_event((card*)0, EVENT_RECOVER, reason_effect, reason, reason_player, playerid, amount);
		process_instant_event();
		return FALSE;
	}
	case 2: {
		returns.ivalue[0] = amount;
		return TRUE;
	}
	case 3: {
		returns.ivalue[0] = 0;
		return TRUE;
	}
	case 10: {
		//dummy
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::pay_lp_cost(uint32 step, uint8 playerid, uint32 cost) {
	switch(step) {
	case 0: {
		effect_set eset;
		int32 val = cost;
		if(cost == 0) {
			raise_event((card*)0, EVENT_PAY_LPCOST, core.reason_effect, 0, playerid, playerid, cost);
			process_instant_event();
			return TRUE;
		}
		filter_player_effect(playerid, EFFECT_LPCOST_CHANGE, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			pduel->lua->add_param(core.reason_effect, PARAM_TYPE_EFFECT);
			pduel->lua->add_param(playerid, PARAM_TYPE_INT);
			pduel->lua->add_param(val, PARAM_TYPE_INT);
			val = eset[i]->get_value(3);
			if(val <= 0)
				return TRUE;
		}
		core.units.begin()->arg2 = val;
		tevent e;
		e.event_cards = 0;
		e.event_player = playerid;
		e.event_value = val;
		e.reason = 0;
		e.reason_effect = core.reason_effect;
		e.reason_player = playerid;
		core.select_options.clear();
		core.select_effects.clear();
		if(val <= player[playerid].lp) {
			core.select_options.push_back(11);
			core.select_effects.push_back(0);
		}
		auto pr = effects.continuous_effect.equal_range(EFFECT_LPCOST_REPLACE);
		for (; pr.first != pr.second; ++pr.first) {
			effect* peffect = pr.first->second;
			if(peffect->is_activateable(peffect->get_handler_player(), e)) {
				core.select_options.push_back(peffect->description);
				core.select_effects.push_back(peffect);
			}
		}
		if(core.select_options.size() == 0)
			return TRUE;
		if(core.select_options.size() == 1)
			returns.ivalue[0] = 0;
		else if(core.select_effects[0] == 0 && core.select_effects.size() == 2)
			add_process(PROCESSOR_SELECT_EFFECTYN, 0, 0, (group*)core.select_effects[1]->handler, playerid, 0);
		else
			add_process(PROCESSOR_SELECT_OPTION, 0, 0, 0, playerid, 0);
		return FALSE;
	}
	case 1: {
		effect* peffect = core.select_effects[returns.ivalue[0]];
		if(!peffect) {
			player[playerid].lp -= cost;
			pduel->write_buffer8(MSG_PAY_LPCOST);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(cost);
			raise_event((card*)0, EVENT_PAY_LPCOST, core.reason_effect, 0, playerid, playerid, cost);
			process_instant_event();
			return TRUE;
		}
		tevent e;
		e.event_cards = 0;
		e.event_player = playerid;
		e.event_value = cost;
		e.reason = 0;
		e.reason_effect = core.reason_effect;
		e.reason_player = playerid;
		core.sub_solving_event.push_back(e);
		add_process(PROCESSOR_SOLVE_CONTINUOUS, 0, peffect, 0, playerid, 0);
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::remove_counter(uint16 step, uint32 reason, card* pcard, uint8 rplayer, uint8 s, uint8 o, uint16 countertype, uint16 count) {
	switch(step) {
	case 0: {
		core.select_options.clear();
		core.select_effects.clear();
		if((pcard && pcard->get_counter(countertype) >= count) || (!pcard && get_field_counter(rplayer, s, o, countertype))) {
			core.select_options.push_back(10);
			core.select_effects.push_back(0);
		}
		auto pr = effects.continuous_effect.equal_range(EFFECT_RCOUNTER_REPLACE + countertype);
		effect* peffect;
		tevent e;
		e.event_cards = 0;
		e.event_player = rplayer;
		e.event_value = count;
		e.reason = reason;
		e.reason_effect = core.reason_effect;
		e.reason_player = rplayer;
		for (; pr.first != pr.second; ++pr.first) {
			peffect = pr.first->second;
			if(peffect->is_activateable(peffect->get_handler_player(), e)) {
				core.select_options.push_back(peffect->description);
				core.select_effects.push_back(peffect);
			}
		}
		returns.ivalue[0] = 0;
		if(core.select_options.size() == 0)
			return TRUE;
		if(core.select_options.size() == 1)
			returns.ivalue[0] = 0;
		else if(core.select_effects[0] == 0 && core.select_effects.size() == 2)
			add_process(PROCESSOR_SELECT_EFFECTYN, 0, 0, (group*)core.select_effects[1]->handler, rplayer, 0);
		else
			add_process(PROCESSOR_SELECT_OPTION, 0, 0, 0, rplayer, 0);
		return FALSE;
	}
	case 1: {
		effect* peffect = core.select_effects[returns.ivalue[0]];
		if(peffect) {
			tevent e;
			e.event_cards = 0;
			e.event_player = rplayer;
			e.event_value = count;
			e.reason = reason;
			e.reason_effect = core.reason_effect;
			e.reason_player = rplayer;
			core.sub_solving_event.push_back(e);
			add_process(PROCESSOR_SOLVE_CONTINUOUS, 0, peffect, 0, rplayer, 0);
			core.units.begin()->step = 3;
			return FALSE;
		}
		if(pcard) {
			returns.ivalue[0] = pcard->remove_counter(countertype, count);
			core.units.begin()->step = 3;
			return FALSE;
		}
		card* pcard;
		core.select_cards.clear();
		uint8 fc = s;
		uint8 fp = rplayer;
		for(int p = 0; p < 2; ++p) {
			if(fc) {
				for(uint32 j = 0; j < 5; ++j) {
					pcard = player[fp].list_mzone[j];
					if(pcard && pcard->get_counter(countertype)) {
						core.select_cards.push_back(pcard);
						pcard->operation_param = pcard->get_counter(countertype);
					}
				}
				for(uint32 j = 0; j < 8; ++j) {
					pcard = player[fp].list_szone[j];
					if(pcard && pcard->get_counter(countertype)) {
						core.select_cards.push_back(pcard);
						pcard->operation_param = pcard->get_counter(countertype);
					}
				}
			}
			fp = 1 - fp;
			fc = o;
		}
		add_process(PROCESSOR_SELECT_COUNTER, 0, 0, 0, rplayer, countertype + (((uint32)count) << 16));
		return FALSE;
	}
	case 2: {
		for(uint32 i = 0; i < core.select_cards.size(); ++i)
			if(returns.bvalue[i] > 0)
				core.select_cards[i]->remove_counter(countertype, returns.bvalue[i]);
		return FALSE;
	}
	case 3: {
		raise_event((card*)0, EVENT_REMOVE_COUNTER + countertype, core.reason_effect, reason, rplayer, rplayer, count);
		process_instant_event();
		return FALSE;
	}
	case 4: {
		returns.ivalue[0] = 1;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::remove_overlay_card(uint16 step, uint32 reason, card* pcard, uint8 rplayer, uint8 s, uint8 o, uint16 min, uint16 max) {
	switch(step) {
	case 0: {
		core.select_options.clear();
		core.select_effects.clear();
		if((pcard && pcard->xyz_materials.size() >= min) || (!pcard && get_overlay_count(rplayer, s, o) >= min)) {
			core.select_options.push_back(12);
			core.select_effects.push_back(0);
		}
		auto pr = effects.continuous_effect.equal_range(EFFECT_OVERLAY_REMOVE_REPLACE);
		effect* peffect;
		tevent e;
		e.event_cards = 0;
		e.event_player = rplayer;
		e.event_value = min;
		e.reason = reason;
		e.reason_effect = core.reason_effect;
		e.reason_player = rplayer;
		for (; pr.first != pr.second; ++pr.first) {
			peffect = pr.first->second;
			if(peffect->is_activateable(peffect->get_handler_player(), e)) {
				core.select_options.push_back(peffect->description);
				core.select_effects.push_back(peffect);
			}
		}
		returns.ivalue[0] = 0;
		if(core.select_options.size() == 0)
			return TRUE;
		if(core.select_options.size() == 1)
			returns.ivalue[0] = 0;
		else if(core.select_effects[0] == 0 && core.select_effects.size() == 2)
			add_process(PROCESSOR_SELECT_EFFECTYN, 0, 0, (group*)core.select_effects[1]->handler, rplayer, 0);
		else
			add_process(PROCESSOR_SELECT_OPTION, 0, 0, 0, rplayer, 0);
		return FALSE;
	}
	case 1: {
		effect* peffect = core.select_effects[returns.ivalue[0]];
		if(peffect) {
			tevent e;
			e.event_cards = 0;
			e.event_player = rplayer;
			e.event_value = min + (max << 16);
			e.reason = reason;
			e.reason_effect = core.reason_effect;
			e.reason_player = rplayer;
			core.sub_solving_event.push_back(e);
			add_process(PROCESSOR_SOLVE_CONTINUOUS, 0, peffect, 0, rplayer, 0);
			core.units.begin()->step = 3;
			return FALSE;
		}
		core.select_cards.clear();
		if(pcard) {
			for(auto cit = pcard->xyz_materials.begin(); cit != pcard->xyz_materials.end(); ++cit)
				core.select_cards.push_back(*cit);
		} else {
			card_set cset;
			get_overlay_group(rplayer, s, o, &cset);
			for(auto cit = cset.begin(); cit != cset.end(); ++cit)
				core.select_cards.push_back(*cit);
		}
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(rplayer);
		pduel->write_buffer32(519);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, rplayer, min + (max << 16));
		return FALSE;
	}
	case 2: {
		card_set cset;
		for(int32 i = 0; i < returns.bvalue[0]; ++i)
			cset.insert(core.select_cards[returns.bvalue[i + 1]]);
		send_to(&cset, core.reason_effect, reason, rplayer, PLAYER_NONE, LOCATION_GRAVE, 0, POS_FACEUP);
		return FALSE;
	}
	case 3: {
		return FALSE;
	}
	case 4: {
		returns.ivalue[0] = 1;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::get_control(uint16 step, effect* reason_effect, uint8 reason_player, group* targets, uint8 playerid, uint16 reset_phase, uint8 reset_count) {
	switch(step) {
	case 0: {
		card_set* destroy_set = new card_set;
		core.units.begin()->arg1 = (ptr)destroy_set;
		for(auto cit = targets->container.begin(); cit != targets->container.end();) {
			auto rm = cit++;
			card* pcard = *rm;
			pcard->filter_disable_related_cards();
			bool change = true;
			if(pcard->overlay_target)
				change = false;
			if(pcard->current.controler == playerid)
				change = false;
			if(pcard->current.controler == PLAYER_NONE)
				change = false;
			if(pcard->current.location != LOCATION_MZONE)
				change = false;
			if(!pcard->is_capable_change_control())
				change = false;
			if(!pcard->is_affect_by_effect(reason_effect))
				change = false;
			if((pcard->get_type() & TYPE_TRAPMONSTER) && get_useable_count(playerid, LOCATION_SZONE, playerid, LOCATION_REASON_CONTROL) <= 0)
				change = false;
			if(!change)
				targets->container.erase(pcard);
		}
		int32 fcount = get_useable_count(playerid, LOCATION_MZONE, playerid, LOCATION_REASON_CONTROL);
		if(fcount <= 0) {
			destroy_set->swap(targets->container);
			core.units.begin()->step = 5;
			return FALSE;
		}
		if((int32)targets->container.size() > fcount) {
			core.select_cards.clear();
			for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit)
				core.select_cards.push_back(*cit);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_SELECTMSG);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(502);
			uint32 ct = targets->container.size() - fcount;
			add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid, ct + (ct << 16));
		} else
			core.units.begin()->step = 1;
		return FALSE;
	}
	case 1: {
		card_set* destroy_set = (card_set*)core.units.begin()->arg1;
		for(int32 i = 0; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			destroy_set->insert(pcard);
			targets->container.erase(pcard);
		}
		return FALSE;
	}
	case 2: {
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			if((*cit)->unique_code && ((*cit)->unique_location & LOCATION_MZONE))
				remove_unique_card(*cit);
		}
		targets->it = targets->container.begin();
		return FALSE;
	}
	case 3: {
		if(targets->it == targets->container.end()) {
			core.units.begin()->step = 4;
			return FALSE;
		}
		card* pcard = *targets->it;
		move_to_field(pcard, playerid, playerid, LOCATION_MZONE, pcard->current.position);
		return FALSE;
	}
	case 4: {
		card* pcard = *targets->it;
		pcard->set_status(STATUS_ATTACK_CANCELED, TRUE);
		set_control(pcard, playerid, reset_phase, reset_count);
		pcard->reset(RESET_CONTROL, RESET_EVENT);
		pcard->filter_disable_related_cards();
		++targets->it;
		core.units.begin()->step = 2;
		return FALSE;
	}
	case 5: {
		adjust_instant();
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ) {
			card* pcard = *cit++;
			if(!(pcard->current.location & LOCATION_ONFIELD)) {
				targets->container.erase(pcard);
				continue;
			}
			if(pcard->unique_code && (pcard->unique_location & LOCATION_MZONE))
				add_unique_card(pcard);
			raise_single_event(pcard, 0, EVENT_CONTROL_CHANGED, reason_effect, REASON_EFFECT, reason_player, playerid, 0);
		}
		if(targets->container.size())
			raise_event(&targets->container, EVENT_CONTROL_CHANGED, reason_effect, REASON_EFFECT, reason_player, playerid, 0);
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 6: {
		card_set* destroy_set = (card_set*)core.units.begin()->arg1;
		if(destroy_set->size())
			destroy(destroy_set, 0, REASON_RULE, PLAYER_NONE);
		delete destroy_set;
		return FALSE;
	}
	case 7: {
		core.operated_set = targets->container;
		returns.ivalue[0] = targets->container.size();
		pduel->delete_group(targets);
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::swap_control(uint16 step, effect * reason_effect, uint8 reason_player, card * pcard1, card * pcard2, uint16 reset_phase, uint8 reset_count) {
	switch(step) {
	case 0: {
		uint8 p1 = pcard1->current.controler, p2 = pcard2->current.controler;
		uint8 l1 = pcard1->current.location, l2 = pcard2->current.location;
		uint8 s1 = pcard1->current.sequence, s2 = pcard2->current.sequence;
		returns.ivalue[0] = 0;
		if(pcard1->overlay_target || pcard2->overlay_target)
			return TRUE;
		if(p1 == p2 || p1 == PLAYER_NONE || p2 == PLAYER_NONE)
			return TRUE;
		if(l1 != LOCATION_MZONE || l2 != LOCATION_MZONE)
			return TRUE;
		if(!pcard1->is_capable_change_control() || !pcard2->is_capable_change_control())
			return TRUE;
		if(!pcard1->is_affect_by_effect(reason_effect) || !pcard2->is_affect_by_effect(reason_effect))
			return TRUE;
		pcard1->filter_disable_related_cards();
		pcard2->filter_disable_related_cards();
		if(pcard1->unique_code && (pcard1->unique_location & LOCATION_MZONE))
			remove_unique_card(pcard1);
		if(pcard2->unique_code && (pcard2->unique_location & LOCATION_MZONE))
			remove_unique_card(pcard2);
		remove_card(pcard1);
		remove_card(pcard2);
		add_card(p2, pcard1, l2, s2);
		add_card(p1, pcard2, l1, s1);
		if(pcard1->unique_code && (pcard1->unique_location & LOCATION_MZONE))
			add_unique_card(pcard1);
		if(pcard2->unique_code && (pcard2->unique_location & LOCATION_MZONE))
			add_unique_card(pcard2);
		set_control(pcard1, p2, reset_phase, reset_count);
		set_control(pcard2, p1, reset_phase, reset_count);
		pcard1->reset(RESET_CONTROL, RESET_EVENT);
		pcard2->reset(RESET_CONTROL, RESET_EVENT);
		pcard1->filter_disable_related_cards();
		pcard2->filter_disable_related_cards();
		pcard1->set_status(STATUS_ATTACK_CANCELED, TRUE);
		pcard2->set_status(STATUS_ATTACK_CANCELED, TRUE);
		return FALSE;
	}
	case 1: {
		pduel->write_buffer8(MSG_SWAP);
		pduel->write_buffer32(pcard1->data.code);
		pduel->write_buffer32(pcard2->get_info_location());
		pduel->write_buffer32(pcard2->data.code);
		pduel->write_buffer32(pcard1->get_info_location());
		adjust_instant();
		raise_single_event(pcard1, 0, EVENT_CONTROL_CHANGED, reason_effect, REASON_EFFECT, reason_player, pcard1->current.controler, 0);
		raise_single_event(pcard2, 0, EVENT_CONTROL_CHANGED, reason_effect, REASON_EFFECT, reason_player, pcard2->current.controler, 0);
		process_single_event();
		card_set cset;
		cset.insert(pcard1);
		cset.insert(pcard2);
		raise_event(&cset, EVENT_CONTROL_CHANGED, reason_effect, REASON_EFFECT, reason_player, 0, 0);
		process_instant_event();
		return FALSE;
	}
	case 2: {
		returns.ivalue[0] = 1;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::control_adjust(uint16 step) {
	switch(step) {
	case 0: {
		card_set* destroy_set = new card_set;
		core.units.begin()->peffect = (effect*)destroy_set;
		uint32 b0 = get_useable_count(0, LOCATION_MZONE, 0, LOCATION_REASON_CONTROL);
		uint32 b1 = get_useable_count(1, LOCATION_MZONE, 1, LOCATION_REASON_CONTROL);
		for(auto cit = core.control_adjust_set[0].begin(); cit != core.control_adjust_set[0].end(); ++cit)
			(*cit)->filter_disable_related_cards();
		for(auto cit = core.control_adjust_set[1].begin(); cit != core.control_adjust_set[1].end(); ++cit)
			(*cit)->filter_disable_related_cards();
		if(core.control_adjust_set[0].size() > core.control_adjust_set[1].size()) {
			if(core.control_adjust_set[0].size() - core.control_adjust_set[1].size() > b1) {
				if(core.control_adjust_set[1].size() == 0 && b1 == 0) {
					destroy_set->swap(core.control_adjust_set[0]);
					core.units.begin()->step = 4;
				} else {
					core.units.begin()->arg1 = 0;
					uint32 count = core.control_adjust_set[0].size() - core.control_adjust_set[1].size() - b1;
					core.select_cards.clear();
					for(auto cit = core.control_adjust_set[0].begin(); cit != core.control_adjust_set[0].end(); ++cit)
						core.select_cards.push_back(*cit);
					pduel->write_buffer8(MSG_HINT);
					pduel->write_buffer8(HINT_SELECTMSG);
					pduel->write_buffer8(infos.turn_player);
					pduel->write_buffer32(502);
					add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, 1, count + (count << 16));
				}
			} else
				core.units.begin()->step = 1;
		} else if (core.control_adjust_set[0].size() < core.control_adjust_set[1].size()) {
			if(core.control_adjust_set[1].size() - core.control_adjust_set[0].size() > b0) {
				if(core.control_adjust_set[0].size() == 0 && b0 == 0) {
					destroy_set->swap(core.control_adjust_set[1]);
					core.units.begin()->step = 4;
				} else {
					core.units.begin()->arg1 = 1;
					uint32 count = core.control_adjust_set[1].size() - core.control_adjust_set[0].size() - b0;
					core.select_cards.clear();
					for(auto cit = core.control_adjust_set[1].begin(); cit != core.control_adjust_set[1].end(); ++cit)
						core.select_cards.push_back(*cit);
					pduel->write_buffer8(MSG_HINT);
					pduel->write_buffer8(HINT_SELECTMSG);
					pduel->write_buffer8(infos.turn_player);
					pduel->write_buffer32(502);
					add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, 0, count + (count << 16));
				}
			} else
				core.units.begin()->step = 1;
		} else
			core.units.begin()->step = 1;
		return FALSE;
	}
	case 1: {
		card_set* destroy_set = (card_set*)core.units.begin()->peffect;
		int32 adjp = core.units.begin()->arg1;
		for(int32 i = 0; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			destroy_set->insert(pcard);
			core.control_adjust_set[adjp].erase(pcard);
		}
		return FALSE;
	}
	case 2: {
		for(auto cit = core.control_adjust_set[0].begin(); cit != core.control_adjust_set[0].end(); ++cit) {
			if((*cit)->unique_code && ((*cit)->unique_location & LOCATION_MZONE))
				remove_unique_card(*cit);
		}
		for(auto cit = core.control_adjust_set[1].begin(); cit != core.control_adjust_set[1].end(); ++cit) {
			if((*cit)->unique_code && ((*cit)->unique_location & LOCATION_MZONE))
				remove_unique_card(*cit);
		}
		auto cit1 = core.control_adjust_set[0].begin();
		auto cit2 = core.control_adjust_set[1].begin();
		while(cit1 != core.control_adjust_set[0].end() && cit2 != core.control_adjust_set[1].end()) {
			card* pcard1 = *cit1++;
			card* pcard2 = *cit2++;
			uint8 p1 = pcard1->current.controler, p2 = pcard2->current.controler;
			uint8 l1 = pcard1->current.location, l2 = pcard2->current.location;
			uint8 s1 = pcard1->current.sequence, s2 = pcard2->current.sequence;
			remove_card(pcard1);
			remove_card(pcard2);
			add_card(p2, pcard1, l2, s2);
			add_card(p1, pcard2, l1, s1);
			pcard1->reset(RESET_CONTROL, RESET_EVENT);
			pcard2->reset(RESET_CONTROL, RESET_EVENT);
			pduel->write_buffer8(MSG_SWAP);
			pduel->write_buffer32(pcard1->data.code);
			pduel->write_buffer32(pcard2->get_info_location());
			pduel->write_buffer32(pcard2->data.code);
			pduel->write_buffer32(pcard1->get_info_location());
		}
		card_set* adjust_set = new card_set;
		core.units.begin()->ptarget = (group*)adjust_set;
		adjust_set->insert(cit1, core.control_adjust_set[0].end());
		adjust_set->insert(cit2, core.control_adjust_set[1].end());
		return FALSE;
	}
	case 3: {
		card_set* adjust_set = (card_set*)core.units.begin()->ptarget;
		if(adjust_set->empty())
			return FALSE;
		auto cit = adjust_set->begin();
		card* pcard = *cit;
		adjust_set->erase(cit);
		pcard->reset(RESET_CONTROL, RESET_EVENT);
		move_to_field(pcard, 1 - pcard->current.controler, 1 - pcard->current.controler, LOCATION_MZONE, pcard->current.position);
		core.units.begin()->step = 2;
		return FALSE;
	}
	case 4: {
		card_set* adjust_set = (card_set*)core.units.begin()->ptarget;
		delete adjust_set;
		core.control_adjust_set[0].insert(core.control_adjust_set[1].begin(), core.control_adjust_set[1].end());
		for(auto cit = core.control_adjust_set[0].begin(); cit != core.control_adjust_set[0].end(); ) {
			card* pcard = *cit++;
			if(!(pcard->current.location & LOCATION_ONFIELD)) {
				core.control_adjust_set[0].erase(pcard);
				continue;
			}
			pcard->filter_disable_related_cards();
			if(pcard->unique_code && (pcard->unique_location & LOCATION_MZONE))
				add_unique_card(pcard);
			raise_single_event(pcard, 0, EVENT_CONTROL_CHANGED, 0, REASON_RULE, 0, 0, 0);
		}
		if(core.control_adjust_set[0].size())
			raise_event(&core.control_adjust_set[0], EVENT_CONTROL_CHANGED, 0, 0, 0, 0, 0);
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 5: {
		card_set* destroy_set = (card_set*)core.units.begin()->peffect;
		if(destroy_set->size())
			destroy(destroy_set, 0, REASON_RULE, PLAYER_NONE);
		delete destroy_set;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::self_destroy(uint16 step) {
	switch(step) {
	case 0: {
		if(core.self_destroy_set.empty()) {
			core.units.begin()->step = 1;
			return FALSE;
		}
		card_set cset;
		for (auto cit = core.self_destroy_set.begin(); cit != core.self_destroy_set.end();) {
			auto rm = cit++;
			card* pcard = *rm;
			if(pcard->current.reason_effect->code == EFFECT_UNIQUE_CHECK) {
				cset.insert(pcard);
				core.self_destroy_set.erase(rm);
			}
		}
		if(!cset.empty())
			destroy(&cset, 0, REASON_RULE, 5);
		return FALSE;
	}
	case 1: {
		core.operated_set.clear();
		if(!core.self_destroy_set.empty())
			destroy(&core.self_destroy_set, 0, REASON_EFFECT, 5);
		return FALSE;
	}
	case 2: {
		core.self_destroy_set.clear();
		core.operated_set.clear();
		returns.ivalue[0] = 0;
		if(!(core.global_flag & GLOBALFLAG_SELF_TOGRAVE))
			return TRUE;
		if(!core.self_tograve_set.empty())
			send_to(&core.self_tograve_set, 0, REASON_EFFECT, PLAYER_NONE, PLAYER_NONE, LOCATION_GRAVE, 0, POS_FACEUP);
		else
			return TRUE;
		return FALSE;
	}
	case 3: {
		core.self_tograve_set.clear();
		core.operated_set.clear();
		returns.ivalue[0] = 0;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::equip(uint16 step, uint8 equip_player, card * equip_card, card * target, uint32 up, uint32 is_step) {
	switch(step) {
	case 0: {
		returns.ivalue[0] = FALSE;
		if(!equip_card->is_affect_by_effect(core.reason_effect))
			return TRUE;
		if(equip_card == target || target->current.location != LOCATION_MZONE)
			return TRUE;
		if(equip_card->equiping_target) {
			equip_card->unequip();
			equip_card->enable_field_effect(false);
			return FALSE;
		}
		if(equip_card->current.location == LOCATION_SZONE) {
			if(up && equip_card->is_position(POS_FACEDOWN))
				change_position(equip_card, 0, equip_player, POS_FACEUP, 0);
			return FALSE;
		}
		refresh_location_info_instant();
		if(get_useable_count(equip_player, LOCATION_SZONE, equip_player, LOCATION_REASON_TOFIELD) <= 0)
			return TRUE;
		equip_card->enable_field_effect(false);
		move_to_field(equip_card, equip_player, equip_player, LOCATION_SZONE, (up || equip_card->is_position(POS_FACEUP)) ? POS_FACEUP : POS_FACEDOWN, FALSE, FALSE, TRUE);
		return FALSE;
	}
	case 1: {
		equip_card->equip(target);
		if(!(equip_card->data.type & TYPE_EQUIP)) {
			effect* peffect = pduel->new_effect();
			peffect->owner = equip_card;
			peffect->handler = equip_card;
			peffect->type = EFFECT_TYPE_SINGLE;
			if(equip_card->get_type() & TYPE_TRAP) {
				peffect->code = EFFECT_ADD_TYPE;
				peffect->value = TYPE_EQUIP;
			} else if(equip_card->data.type & TYPE_UNION) {
				peffect->code = EFFECT_CHANGE_TYPE;
				peffect->value = TYPE_EQUIP + TYPE_SPELL + TYPE_UNION;
			} else {
				peffect->code = EFFECT_CHANGE_TYPE;
				peffect->value = TYPE_EQUIP + TYPE_SPELL;
			}
			peffect->flag[0] = EFFECT_FLAG_CANNOT_DISABLE;
			peffect->reset_flag = RESET_EVENT + 0x17e0000;
			equip_card->add_effect(peffect);
		}
		equip_card->effect_target_cards.insert(target);
		target->effect_target_owner.insert(equip_card);
		if(!is_step) {
			if(equip_card->is_position(POS_FACEUP))
				equip_card->enable_field_effect(true);
			adjust_disable_check_list();
			card_set cset;
			cset.insert(equip_card);
			raise_single_event(target, &cset, EVENT_EQUIP, core.reason_effect, 0, core.reason_player, PLAYER_NONE, 0);
			raise_event(&cset, EVENT_EQUIP, core.reason_effect, 0, core.reason_player, PLAYER_NONE, 0);
			core.hint_timing[target->current.controler] |= TIMING_EQUIP;
			process_single_event();
			process_instant_event();
			return FALSE;
		} else {
			core.equiping_cards.insert(equip_card);
			returns.ivalue[0] = TRUE;
			return TRUE;
		}
	}
	case 2: {
		returns.ivalue[0] = TRUE;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::summon(uint16 step, uint8 sumplayer, card * target, effect * proc, uint8 ignore_count, uint8 min_tribute) {
	switch(step) {
	case 0: {
		if(!(target->data.type & TYPE_MONSTER))
			return TRUE;
		if(target->current.location == LOCATION_MZONE) {
			if(target->is_position(POS_FACEDOWN))
				return TRUE;
			if(!ignore_count && (core.extra_summon[sumplayer] || !target->is_affected_by_effect(EFFECT_EXTRA_SUMMON_COUNT))
			        && (core.summon_count[sumplayer] >= get_summon_count_limit(sumplayer)))
				return TRUE;
			if(!target->is_affected_by_effect(EFFECT_DUAL_SUMMONABLE))
				return TRUE;
			if(target->is_affected_by_effect(EFFECT_DUAL_STATUS))
				return TRUE;
			if(!is_player_can_summon(SUMMON_TYPE_DUAL, sumplayer, target))
				return TRUE;
		} else {
			if(!ignore_count && !core.extra_summon[sumplayer] && core.summon_count[sumplayer] >= get_summon_count_limit(sumplayer)) {
				effect* pextra = target->is_affected_by_effect(EFFECT_EXTRA_SUMMON_COUNT);
				if(pextra && !pextra->is_flag(EFFECT_FLAG_FUNC_VALUE)) {
					int32 count = pextra->get_value();
					if(min_tribute < count) {
						min_tribute = count;
						core.units.begin()->arg1 = sumplayer + (ignore_count << 8) + (min_tribute << 16);
					}
				}
			}
			effect_set eset;
			int32 res = target->filter_summon_procedure(sumplayer, &eset, ignore_count, min_tribute);
			if((proc && res < 0) || res == -2)
				return TRUE;
		}
		if(check_unique_onfield(target, sumplayer, LOCATION_MZONE))
			return TRUE;
		if(target->is_affected_by_effect(EFFECT_CANNOT_SUMMON))
			return TRUE;
		if(core.summon_depth)
			core.summon_cancelable = FALSE;
		core.summon_depth++;
		target->material_cards.clear();
		if(target->current.location == LOCATION_MZONE)
			core.units.begin()->step = 3;
		return FALSE;
	}
	case 1: {
		if(proc) {
			core.units.begin()->step = 3;
			return FALSE;
		}
		effect_set eset;
		int32 res = target->filter_summon_procedure(sumplayer, &eset, ignore_count, min_tribute);
		core.select_effects.clear();
		core.select_options.clear();
		if(res > 0) {
			core.select_effects.push_back(0);
			core.select_options.push_back(1);
		}
		for(int32 i = 0; i < eset.size(); ++i) {
			core.select_effects.push_back(eset[i]);
			core.select_options.push_back(eset[i]->description);
		}
		if(core.select_options.size() == 1)
			returns.ivalue[0] = 0;
		else
			add_process(PROCESSOR_SELECT_OPTION, 0, 0, 0, sumplayer, 0);
		return FALSE;
	}
	case 2: {
		effect* peffect = core.select_effects[returns.ivalue[0]];
		if(peffect) {
			core.units.begin()->peffect = peffect;
			core.units.begin()->step = 3;
			return FALSE;
		}
		core.select_cards.clear();
		int32 required = target->get_summon_tribute_count();
		int32 min = required & 0xffff;
		int32 max = required >> 16;
		if(min < min_tribute) {
			min = min_tribute;
			required = min + (max << 16);
		}
		uint32 adv = is_player_can_summon(SUMMON_TYPE_ADVANCE, sumplayer, target);
		if(max == 0 || !adv) {
			returns.bvalue[0] = 0;
			core.units.begin()->step = 3;
		} else {
			core.release_cards.clear();
			core.release_cards_ex.clear();
			core.release_cards_ex_sum.clear();
			int32 rcount = get_summon_release_list(target, &core.release_cards, &core.release_cards_ex, &core.release_cards_ex_sum);
			if(rcount == 0) {
				returns.bvalue[0] = 0;
				core.units.begin()->step = 3;
			} else {
				int32 fcount = get_useable_count(sumplayer, LOCATION_MZONE, sumplayer, LOCATION_REASON_TOFIELD);
				if(min == 0 && fcount > 0) {
					add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, sumplayer, 90);
					core.units.begin()->arg2 = required;
				} else {
					if(min < -fcount + 1) {
						min = -fcount + 1;
						required = min + (max << 16);
					}
					add_process(PROCESSOR_SELECT_TRIBUTE, 0, 0, 0, sumplayer + ((uint32)core.summon_cancelable << 16), required);
					core.units.begin()->step = 3;
				}
			}
		}
		return FALSE;
	}
	case 3: {
		if(returns.ivalue[0])
			returns.bvalue[0] = 0;
		else {
			int32 required = 1 + core.units.begin()->arg2;
			add_process(PROCESSOR_SELECT_TRIBUTE, 0, 0, 0, sumplayer + ((uint32)core.summon_cancelable << 16), required);
		}
		return FALSE;
	}
	case 4: {
		if(target->current.location == LOCATION_MZONE)
			core.units.begin()->step = 8;
		else if(proc)
			core.units.begin()->step = 5;
		else {
			if(returns.ivalue[0] == -1) {
				core.summon_depth--;
				return TRUE;
			}
			if(returns.bvalue[0]) {
				card_set* tributes = new card_set;
				for(int32 i = 0; i < returns.bvalue[0]; ++i)
					tributes->insert(core.select_cards[returns.bvalue[i + 1]]);
				core.units.begin()->peffect = (effect*)tributes;
			}
		}
		effect_set eset;
		target->filter_effect(EFFECT_SUMMON_COST, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			if(eset[i]->operation) {
				core.sub_solving_event.push_back(nil_event);
				add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, sumplayer, 0);
			}
		}
		return FALSE;
	}
	case 5: {
		card_set* tributes = (card_set*)proc;
		int32 min = 0;
		int32 level = target->get_level();
		if(level < 5)
			min = 0;
		else if(level < 7)
			min = 1;
		else
			min = 2;
		if(tributes)
			min -= tributes->size();
		if(min > 0) {
			effect_set eset;
			target->filter_effect(EFFECT_DECREASE_TRIBUTE, &eset);
			int32 minul = 0;
			effect* pdec = 0;
			for(int32 i = 0; i < eset.size(); ++i) {
				if(!eset[i]->is_flag(EFFECT_FLAG_COUNT_LIMIT)) {
					int32 dec = eset[i]->get_value(target);
					if(minul < (dec & 0xffff)) {
						minul = dec & 0xffff;
						pdec = eset[i];
					}
				}
			}
			if(pdec) {
				min -= minul;
				pduel->write_buffer8(MSG_HINT);
				pduel->write_buffer8(HINT_CARD);
				pduel->write_buffer8(0);
				pduel->write_buffer32(pdec->handler->data.code);
			}
			for(int32 i = 0; i < eset.size() && min > 0; ++i) {
				if(eset[i]->is_flag(EFFECT_FLAG_COUNT_LIMIT) && (eset[i]->reset_count & 0xf00) > 0 && eset[i]->target) {
					int32 dec = eset[i]->get_value(target);
					min -= dec & 0xffff;
					eset[i]->dec_count();
					pduel->write_buffer8(MSG_HINT);
					pduel->write_buffer8(HINT_CARD);
					pduel->write_buffer8(0);
					pduel->write_buffer32(eset[i]->handler->data.code);
				}
			}
			for(int32 i = 0; i < eset.size() && min > 0; ++i) {
				if(eset[i]->is_flag(EFFECT_FLAG_COUNT_LIMIT) && (eset[i]->reset_count & 0xf00) > 0 && !eset[i]->target) {
					int32 dec = eset[i]->get_value(target);
					min -= dec & 0xffff;
					eset[i]->dec_count();
					pduel->write_buffer8(MSG_HINT);
					pduel->write_buffer8(HINT_CARD);
					pduel->write_buffer8(0);
					pduel->write_buffer32(eset[i]->handler->data.code);
				}
			}
		}
		effect* pextra = 0;
		if(!ignore_count && !core.extra_summon[sumplayer])
			pextra = target->is_affected_by_effect(EFFECT_EXTRA_SUMMON_COUNT);
		if(tributes) {
			for(auto cit = tributes->begin(); cit != tributes->end(); ++cit)
				(*cit)->current.reason_card = target;
			target->set_material(tributes);
			release(tributes, 0, REASON_SUMMON | REASON_MATERIAL, sumplayer);
			target->summon_info = SUMMON_TYPE_NORMAL | SUMMON_TYPE_ADVANCE | (LOCATION_HAND << 16);
			delete tributes;
			core.units.begin()->peffect = 0;
			adjust_all();
		} else
			target->summon_info = SUMMON_TYPE_NORMAL | (LOCATION_HAND << 16);
		target->current.reason_effect = 0;
		target->current.reason_player = sumplayer;
		core.units.begin()->step = 6;
		core.units.begin()->arg2 = (ptr)pextra;
		return FALSE;
	}
	case 6: {
		effect* pextra = 0;
		if(!ignore_count && !core.extra_summon[sumplayer])
			pextra = target->is_affected_by_effect(EFFECT_EXTRA_SUMMON_COUNT);
		target->summon_info = (proc->get_value(target) & 0xfffffff) | SUMMON_TYPE_NORMAL | (LOCATION_HAND << 16);
		target->current.reason_effect = proc;
		target->current.reason_player = sumplayer;
		if(proc->operation) {
			pduel->lua->add_param(target, PARAM_TYPE_CARD);
			core.sub_solving_event.push_back(nil_event);
			add_process(PROCESSOR_EXECUTE_OPERATION, 0, proc, 0, sumplayer, 0);
		}
		proc->dec_count(sumplayer);
		core.units.begin()->arg2 = (ptr)pextra;
		return FALSE;
	}
	case 7: {
		core.summon_depth--;
		if(core.summon_depth)
			return TRUE;
		break_effect();
		if(!ignore_count) {
			returns.ivalue[0] = FALSE;
			effect* pextra = (effect*)core.units.begin()->arg2;
			if(pextra) {
				if(pextra->is_flag(EFFECT_FLAG_FUNC_VALUE) && (core.summon_count[sumplayer] < get_summon_count_limit(sumplayer)))
					add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, sumplayer, 91);
				else if(!pextra->is_flag(EFFECT_FLAG_FUNC_VALUE) && ((int32)target->material_cards.size() < pextra->get_value()))
					core.units.begin()->arg2 = 0;
				else
					returns.ivalue[0] = TRUE;
			}
		} else
			returns.ivalue[0] = TRUE;
		return FALSE;
	}
	case 8: {
		if(!returns.ivalue[0])
			core.summon_count[sumplayer]++;
		else if(core.units.begin()->arg2) {
			core.extra_summon[sumplayer] = TRUE;
			effect* pextra = (effect*)core.units.begin()->arg2;
			pextra->get_value(target);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_CARD);
			pduel->write_buffer8(0);
			pduel->write_buffer32(pextra->handler->data.code);
		}
		uint8 targetplayer = sumplayer;
		uint8 positions = POS_FACEUP_ATTACK;
		if(is_player_affected_by_effect(sumplayer, EFFECT_DEVINE_LIGHT))
			positions = POS_FACEUP;
		if(proc && proc->is_flag(EFFECT_FLAG_SPSUM_PARAM)) {
			positions = (uint8)proc->s_range;
			if(proc->o_range)
				targetplayer = 1 - sumplayer;
		}
		target->enable_field_effect(false);
		move_to_field(target, sumplayer, targetplayer, LOCATION_MZONE, positions);
		core.summoning_card = target;
		core.units.begin()->step = 10;
		return FALSE;
	}
	case 9: {
		core.summon_depth--;
		if(core.summon_depth)
			return TRUE;
		target->enable_field_effect(false);
		target->summon_info |= SUMMON_TYPE_NORMAL;
		target->current.reason_effect = 0;
		target->current.reason_player = sumplayer;
		effect* deffect = pduel->new_effect();
		deffect->owner = target;
		deffect->code = EFFECT_DUAL_STATUS;
		deffect->type = EFFECT_TYPE_SINGLE;
		deffect->flag[0] = EFFECT_FLAG_CANNOT_DISABLE | EFFECT_FLAG_CLIENT_HINT;
		deffect->description = 64;
		deffect->reset_flag = RESET_EVENT + 0x1fe0000;
		target->add_effect(deffect);
		core.units.begin()->arg2 = 0;
		if(!ignore_count) {
			returns.ivalue[0] = FALSE;
			effect* pextra = core.extra_summon[sumplayer] ? 0 : target->is_affected_by_effect(EFFECT_EXTRA_SUMMON_COUNT);
			if(pextra) {
				core.units.begin()->arg2 = (ptr)pextra;
				if(pextra->is_flag(EFFECT_FLAG_FUNC_VALUE) && (core.summon_count[sumplayer] < get_summon_count_limit(sumplayer)))
					add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, sumplayer, 91);
				else
					returns.ivalue[0] = TRUE;
			}
		} else
			returns.ivalue[0] = TRUE;
		return FALSE;
	}
	case 10: {
		if(!returns.ivalue[0])
			core.summon_count[sumplayer]++;
		else if(core.units.begin()->arg2) {
			core.extra_summon[sumplayer] = TRUE;
			effect* pextra = (effect*)core.units.begin()->arg2;
			pextra->get_value(target);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_CARD);
			pduel->write_buffer8(0);
			pduel->write_buffer32(pextra->handler->data.code);
		}
		core.summoning_card = target;
		return FALSE;
	}
	case 11: {
		uint8 targetplayer = target->current.controler;
		if(target->owner != targetplayer)
			set_control(target, targetplayer, 0, 0);
		core.phase_action = TRUE;
		target->current.reason = REASON_SUMMON;
		target->summon_player = sumplayer;
		pduel->write_buffer8(MSG_SUMMONING);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		core.summon_state_count[sumplayer]++;
		core.normalsummon_state_count[sumplayer]++;
		check_card_counter(target, 1, sumplayer);
		check_card_counter(target, 2, sumplayer);
		if (target->material_cards.size()) {
			for (auto mit = target->material_cards.begin(); mit != target->material_cards.end(); ++mit)
				raise_single_event(*mit, 0, EVENT_BE_PRE_MATERIAL, proc, REASON_SUMMON, sumplayer, sumplayer, 0);
			raise_event(&target->material_cards, EVENT_BE_PRE_MATERIAL, proc, REASON_SUMMON, sumplayer, sumplayer, 0);
		}
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 12: {
		if(core.current_chain.size() == 0) {
			if(target->is_affected_by_effect(EFFECT_CANNOT_DISABLE_SUMMON))
				core.units.begin()->step = 14;
			return FALSE;
		} else if(core.current_chain.size() > 1) {
			core.units.begin()->step = 14;
			return FALSE;
		} else {
			if(target->is_affected_by_effect(EFFECT_CANNOT_DISABLE_SUMMON))
				core.units.begin()->step = 15;
			else
				core.units.begin()->step = 13;
			core.reserved = core.units.front();
			return TRUE;
		}
		return FALSE;
	}
	case 13: {
		target->set_status(STATUS_SUMMONING, TRUE);
		target->set_status(STATUS_SUMMON_DISABLED, FALSE);
		core.summoning_card = 0;
		raise_event(target, EVENT_SUMMON, proc, 0, sumplayer, sumplayer, 0);
		process_instant_event();
		add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, 0x101, TRUE);
		return FALSE;
	}
	case 14: {
		if(target->is_status(STATUS_SUMMONING)) {
			core.units.begin()->step = 14;
			return FALSE;
		}
		if(target->current.location == LOCATION_MZONE)
			send_to(target, 0, REASON_RULE, sumplayer, sumplayer, LOCATION_GRAVE, 0, 0);
		adjust_instant();
		add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		return TRUE;
	}
	case 15: {
		if(proc) {
			for(auto oeit = effects.oath.begin(); oeit != effects.oath.end(); ++oeit)
				if(oeit->second == proc)
					oeit->second = 0;
		}
		target->set_status(STATUS_SUMMONING, FALSE);
		target->set_status(STATUS_SUMMON_TURN, TRUE);
		target->enable_field_effect(true);
		if(target->is_status(STATUS_DISABLED))
			target->reset(RESET_DISABLE, RESET_EVENT);
		core.summoning_card = 0;
		return FALSE;
	}
	case 16: {
		pduel->write_buffer8(MSG_SUMMONED);
		adjust_instant();
		if(target->material_cards.size()) {
			for(auto mit = target->material_cards.begin(); mit != target->material_cards.end(); ++mit)
				raise_single_event(*mit, 0, EVENT_BE_MATERIAL, proc, REASON_SUMMON, sumplayer, sumplayer, 0);
			raise_event(&target->material_cards, EVENT_BE_MATERIAL, proc, REASON_SUMMON, sumplayer, sumplayer, 0);
		}
		process_single_event();
		process_instant_event();
		return false;
	}
	case 17: {
		raise_single_event(target, 0, EVENT_SUMMON_SUCCESS, proc, 0, sumplayer, sumplayer, 0);
		process_single_event();
		raise_event(target, EVENT_SUMMON_SUCCESS, proc, 0, sumplayer, sumplayer, 0);
		process_instant_event();
		if(core.current_chain.size() == 0) {
			adjust_all();
			core.hint_timing[sumplayer] |= TIMING_SUMMON;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		}
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::flip_summon(uint16 step, uint8 sumplayer, card * target) {
	switch(step) {
	case 0: {
		if(target->current.location != LOCATION_MZONE)
			return TRUE;
		if(!(target->current.position & POS_FACEDOWN))
			return TRUE;
		if(check_unique_onfield(target, sumplayer, LOCATION_MZONE))
			return TRUE;
		effect_set eset;
		target->filter_effect(EFFECT_FLIPSUMMON_COST, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			if(eset[i]->operation) {
				core.sub_solving_event.push_back(nil_event);
				add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, sumplayer, 0);
			}
		}
		return FALSE;
	}
	case 1: {
		target->previous.position = target->current.position;
		target->current.position = POS_FACEUP_ATTACK;
		target->fieldid = infos.field_id++;
		core.phase_action = TRUE;
		core.flipsummon_state_count[sumplayer]++;
		check_card_counter(target, 4, sumplayer);
		pduel->write_buffer8(MSG_FLIPSUMMONING);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		if(target->is_affected_by_effect(EFFECT_CANNOT_DISABLE_FLIP_SUMMON))
			core.units.begin()->step = 2;
		else {
			target->set_status(STATUS_SUMMONING, TRUE);
			target->set_status(STATUS_SUMMON_DISABLED, FALSE);
			raise_event(target, EVENT_FLIP_SUMMON, 0, 0, sumplayer, sumplayer, 0);
			process_instant_event();
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, 0x101, TRUE);
		}
		return FALSE;
	}
	case 2: {
		if(target->is_status(STATUS_SUMMONING))
			return FALSE;
		if(target->current.location == LOCATION_MZONE)
			send_to(target, 0, REASON_RULE, sumplayer, sumplayer, LOCATION_GRAVE, 0, 0);
		add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		return TRUE;
	}
	case 3: {
		target->set_status(STATUS_SUMMONING, FALSE);
		target->enable_field_effect(true);
		if(target->is_status(STATUS_DISABLED))
			target->reset(RESET_DISABLE, RESET_EVENT);
		target->set_status(STATUS_FLIP_SUMMON_TURN, TRUE);
		return FALSE;
	}
	case 4: {
		pduel->write_buffer8(MSG_FLIPSUMMONED);
		adjust_instant();
		raise_single_event(target, 0, EVENT_FLIP, 0, 0, sumplayer, sumplayer, 0);
		raise_single_event(target, 0, EVENT_FLIP_SUMMON_SUCCESS, 0, 0, sumplayer, sumplayer, 0);
		raise_single_event(target, 0, EVENT_CHANGE_POS, 0, 0, sumplayer, sumplayer, 0);
		process_single_event();
		raise_event(target, EVENT_FLIP, 0, 0, sumplayer, sumplayer, 0);
		raise_event(target, EVENT_FLIP_SUMMON_SUCCESS, 0, 0, sumplayer, sumplayer, 0);
		raise_event(target, EVENT_CHANGE_POS, 0, 0, sumplayer, sumplayer, 0);
		process_instant_event();
		adjust_all();
		if(core.current_chain.size() == 0) {
			core.hint_timing[sumplayer] |= TIMING_FLIPSUMMON;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		}
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::mset(uint16 step, uint8 setplayer, card * target, effect * proc, uint8 ignore_count, uint8 min_tribute) {
	switch(step) {
	case 0: {
		if(target->is_affected_by_effect(EFFECT_UNSUMMONABLE_CARD))
			return TRUE;
		if(target->current.location != LOCATION_HAND)
			return TRUE;
		if(!(target->data.type & TYPE_MONSTER))
			return TRUE;
		if(target->is_affected_by_effect(EFFECT_CANNOT_MSET))
			return TRUE;
		if(!ignore_count && (core.extra_summon[setplayer] || !target->is_affected_by_effect(EFFECT_EXTRA_SET_COUNT))
		        && core.summon_count[setplayer] >= get_summon_count_limit(setplayer))
			return TRUE;
		if(!ignore_count && !core.extra_summon[setplayer] && core.summon_count[setplayer] >= get_summon_count_limit(setplayer)) {
			effect* pextra = target->is_affected_by_effect(EFFECT_EXTRA_SET_COUNT);
			if(pextra && !pextra->is_flag(EFFECT_FLAG_FUNC_VALUE)) {
				int32 count = pextra->get_value();
				if(min_tribute < count) {
					min_tribute = count;
					core.units.begin()->arg1 = setplayer + (ignore_count << 8) + (min_tribute << 16);
				}
			}
		}
		target->material_cards.clear();
		effect_set eset;
		int32 res = target->filter_set_procedure(setplayer, &eset, ignore_count, min_tribute);
		if((proc && res < 0) || res == -2)
			return TRUE;
		return FALSE;
	}
	case 1: {
		if(proc) {
			core.units.begin()->step = 3;
			return FALSE;
		}
		effect_set eset;
		int32 res = target->filter_set_procedure(setplayer, &eset, ignore_count, min_tribute);
		core.select_effects.clear();
		core.select_options.clear();
		if(res > 0) {
			core.select_effects.push_back(0);
			core.select_options.push_back(1);
		}
		for(int32 i = 0; i < eset.size(); ++i) {
			core.select_effects.push_back(eset[i]);
			core.select_options.push_back(eset[i]->description);
		}
		if(core.select_options.size() == 1)
			returns.ivalue[0] = 0;
		else
			add_process(PROCESSOR_SELECT_OPTION, 0, 0, 0, setplayer, 0);
		return FALSE;
	}
	case 2: {
		effect* peffect = core.select_effects[returns.ivalue[0]];
		if(peffect) {
			core.units.begin()->peffect = peffect;
			core.units.begin()->step = 3;
			return FALSE;
		}
		core.select_cards.clear();
		int32 required = target->get_set_tribute_count();
		int32 min = required & 0xffff;
		int32 max = required >> 16;
		if(min < min_tribute) {
			min = min_tribute;
			required = min + (max << 16);
		}
		uint32 adv = is_player_can_mset(SUMMON_TYPE_ADVANCE, setplayer, target);
		if(max == 0 || !adv) {
			returns.bvalue[0] = 0;
			core.units.begin()->step = 3;
		} else {
			core.release_cards.clear();
			core.release_cards_ex.clear();
			core.release_cards_ex_sum.clear();
			int32 rcount = get_summon_release_list(target, &core.release_cards, &core.release_cards_ex, &core.release_cards_ex_sum);
			if(rcount == 0) {
				returns.bvalue[0] = 0;
				core.units.begin()->step = 3;
			} else {
				int32 fcount = get_useable_count(setplayer, LOCATION_MZONE, setplayer, LOCATION_REASON_TOFIELD);
				if(min == 0 && fcount > 0) {
					add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, setplayer, 90);
					core.units.begin()->arg2 = required;
				} else {
					if(min < -fcount + 1) {
						min = -fcount + 1;
						required = min + (max << 16);
					}
					add_process(PROCESSOR_SELECT_TRIBUTE, 0, 0, 0, setplayer + ((uint32)core.summon_cancelable << 16), required);
					core.units.begin()->step = 3;
				}
			}
		}
		return FALSE;
	}
	case 3: {
		if(returns.ivalue[0])
			returns.bvalue[0] = 0;
		else {
			int32 required = 1 + core.units.begin()->arg2;
			add_process(PROCESSOR_SELECT_TRIBUTE, 0, 0, 0, setplayer + ((uint32)core.summon_cancelable << 16), required);
		}
		return FALSE;
	}
	case 4: {
		if(proc)
			core.units.begin()->step = 5;
		else {
			if(returns.ivalue[0] == -1) {
				return TRUE;
			}
			if(returns.bvalue[0]) {
				card_set* tributes = new card_set;
				for(int32 i = 0; i < returns.bvalue[0]; ++i)
					tributes->insert(core.select_cards[returns.bvalue[i + 1]]);
				core.units.begin()->peffect = (effect*)tributes;
			}
		}
		effect_set eset;
		target->filter_effect(EFFECT_MSET_COST, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			if(eset[i]->operation) {
				core.sub_solving_event.push_back(nil_event);
				add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, setplayer, 0);
			}
		}
		return FALSE;
	}
	case 5: {
		card_set* tributes = (card_set*)proc;
		effect* pextra = 0;
		if(!ignore_count && !core.extra_summon[setplayer])
			pextra = target->is_affected_by_effect(EFFECT_EXTRA_SET_COUNT);
		if(tributes) {
			for(auto cit = tributes->begin(); cit != tributes->end(); ++cit)
				(*cit)->current.reason_card = target;
			target->set_material(tributes);
			release(tributes, 0, REASON_SUMMON | REASON_MATERIAL, setplayer);
			target->summon_info = SUMMON_TYPE_NORMAL | SUMMON_TYPE_ADVANCE | (LOCATION_HAND << 16);
			delete tributes;
			core.units.begin()->peffect = 0;
			adjust_all();
		} else
			target->summon_info = SUMMON_TYPE_NORMAL | (LOCATION_HAND << 16);
		target->summon_player = setplayer;
		target->current.reason_effect = 0;
		target->current.reason_player = setplayer;
		core.units.begin()->step = 6;
		core.units.begin()->arg2 = (ptr)pextra;
		return FALSE;
	}
	case 6: {
		effect* pextra = 0;
		if(!ignore_count && !core.extra_summon[setplayer])
			pextra = target->is_affected_by_effect(EFFECT_EXTRA_SET_COUNT);
		target->summon_info = (proc->get_value(target) & 0xfffffff) | SUMMON_TYPE_NORMAL | (LOCATION_HAND << 16);
		target->summon_player = setplayer;
		target->current.reason_effect = proc;
		target->current.reason_player = setplayer;
		pduel->lua->add_param(target, PARAM_TYPE_CARD);
		core.sub_solving_event.push_back(nil_event);
		add_process(PROCESSOR_EXECUTE_OPERATION, 0, proc, 0, setplayer, 0);
		proc->dec_count(setplayer);
		core.units.begin()->arg2 = (ptr)pextra;
		return FALSE;
	}
	case 7: {
		break_effect();
		if(!ignore_count) {
			returns.ivalue[0] = FALSE;
			effect* pextra = (effect*)core.units.begin()->arg2;
			if(pextra) {
				if(pextra->is_flag(EFFECT_FLAG_FUNC_VALUE) && (core.summon_count[setplayer] < get_summon_count_limit(setplayer)))
					add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, setplayer, 91);
				else if(!pextra->is_flag(EFFECT_FLAG_FUNC_VALUE) && ((int32)target->material_cards.size() < pextra->get_value()))
					core.units.begin()->arg2 = 0;
				else
					returns.ivalue[0] = TRUE;
			}
		} else
			returns.ivalue[0] = TRUE;
		return FALSE;
	}
	case 8: {
		if(!returns.ivalue[0])
			core.summon_count[setplayer]++;
		else if(core.units.begin()->arg2) {
			core.extra_summon[setplayer] = TRUE;
			effect* pextra = (effect*)core.units.begin()->arg2;
			pextra->get_value(target);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_CARD);
			pduel->write_buffer8(0);
			pduel->write_buffer32(pextra->handler->data.code);
		}
		uint8 targetplayer = setplayer;
		uint8 positions = POS_FACEDOWN_DEFENSE;
		if(proc && proc->is_flag(EFFECT_FLAG_SPSUM_PARAM)) {
			positions = (uint8)proc->s_range;
			if(proc->o_range)
				targetplayer = 1 - setplayer;
		}
		target->enable_field_effect(false);
		move_to_field(target, setplayer, targetplayer, LOCATION_MZONE, positions);
		return FALSE;
	}
	case 9: {
		uint8 targetplayer = target->current.controler;
		if(target->owner != targetplayer)
			set_control(target, targetplayer, 0, 0);
		core.phase_action = TRUE;
		core.normalsummon_state_count[setplayer]++;
		check_card_counter(target, 2, setplayer);
		target->set_status(STATUS_SUMMON_TURN, TRUE);
		pduel->write_buffer8(MSG_SET);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		adjust_instant();
		raise_event(target, EVENT_MSET, proc, 0, setplayer, setplayer, 0);
		process_instant_event();
		if(core.current_chain.size() == 0) {
			adjust_all();
			core.hint_timing[setplayer] |= TIMING_MSET;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, FALSE);
		}
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::sset(uint16 step, uint8 setplayer, uint8 toplayer, card * target) {
	switch(step) {
	case 0: {
		if(!(target->data.type & TYPE_FIELD) && get_useable_count(toplayer, LOCATION_SZONE, setplayer, LOCATION_REASON_TOFIELD) <= 0)
			return TRUE;
		if(target->data.type & TYPE_MONSTER && !target->is_affected_by_effect(EFFECT_MONSTER_SSET))
			return TRUE;
		if(target->current.location == LOCATION_SZONE)
			return TRUE;
		if(!is_player_can_sset(setplayer, target))
			return TRUE;
		if(target->is_affected_by_effect(EFFECT_CANNOT_SSET))
			return TRUE;
		effect_set eset;
		target->filter_effect(EFFECT_SSET_COST, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			if(eset[i]->operation) {
				core.sub_solving_event.push_back(nil_event);
				add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, setplayer, 0);
			}
		}
		return FALSE;
	}
	case 1: {
		target->enable_field_effect(false);
		move_to_field(target, setplayer, toplayer, LOCATION_SZONE, POS_FACEDOWN);
		return FALSE;
	}
	case 2: {
		core.phase_action = TRUE;
		target->set_status(STATUS_SET_TURN, TRUE);
		if(target->data.type & TYPE_MONSTER) {
			effect* peffect = target->is_affected_by_effect(EFFECT_MONSTER_SSET);
			int32 type_val = peffect->get_value();
			peffect = pduel->new_effect();
			peffect->owner = target;
			peffect->type = EFFECT_TYPE_SINGLE;
			peffect->code = EFFECT_CHANGE_TYPE;
			peffect->reset_flag = RESET_EVENT + 0x1fe0000;
			peffect->value = type_val;
			target->add_effect(peffect);
		}
		pduel->write_buffer8(MSG_SET);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		adjust_instant();
		raise_event(target, EVENT_SSET, 0, 0, setplayer, setplayer, 0);
		process_instant_event();
		if(core.current_chain.size() == 0) {
			adjust_all();
			core.hint_timing[setplayer] |= TIMING_SSET;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, FALSE);
		}
	}
	}
	return TRUE;
}
int32 field::sset_g(uint16 step, uint8 setplayer, uint8 toplayer, group* ptarget) {
	switch(step) {
	case 0: {
		card_set* set_cards = new card_set;
		core.operated_set.clear();
		for(auto cit = ptarget->container.begin(); cit != ptarget->container.end(); ++cit) {
			card* target = *cit;
			if((!(target->data.type & TYPE_FIELD) && get_useable_count(toplayer, LOCATION_SZONE, setplayer, LOCATION_REASON_TOFIELD) <= 0)
			        || (target->data.type & TYPE_MONSTER && !target->is_affected_by_effect(EFFECT_MONSTER_SSET))
			        || (target->current.location == LOCATION_SZONE)
			        || (!is_player_can_sset(setplayer, target))
			        || (target->is_affected_by_effect(EFFECT_CANNOT_SSET))) {
				continue;
			}
			set_cards->insert(target);
		}
		if(set_cards->empty()) {
			delete set_cards;
			returns.ivalue[0] = 0;
			return TRUE;
		}
		core.phase_action = TRUE;
		core.units.begin()->ptarget = (group*)set_cards;
		return FALSE;
	}
	case 1: {
		card_set* set_cards = (card_set*)ptarget;
		card* target = *set_cards->begin();
		target->enable_field_effect(false);
		move_to_field(target, setplayer, toplayer, LOCATION_SZONE, POS_FACEDOWN, FALSE);
		return FALSE;
	}
	case 2: {
		card_set* set_cards = (card_set*)ptarget;
		card* target = *set_cards->begin();
		target->set_status(STATUS_SET_TURN, TRUE);
		if(target->data.type & TYPE_MONSTER) {
			effect* peffect = target->is_affected_by_effect(EFFECT_MONSTER_SSET);
			int32 type_val = peffect->get_value();
			peffect = pduel->new_effect();
			peffect->owner = target;
			peffect->type = EFFECT_TYPE_SINGLE;
			peffect->code = EFFECT_CHANGE_TYPE;
			peffect->reset_flag = RESET_EVENT + 0x1fe0000;
			peffect->value = type_val;
			target->add_effect(peffect);
		}
		pduel->write_buffer8(MSG_SET);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		core.operated_set.insert(target);
		set_cards->erase(target);
		if(!set_cards->empty())
			core.units.begin()->step = 0;
		else
			delete set_cards;
		return FALSE;
	}
	case 3: {
		returns.ivalue[0] = core.operated_set.size();
		adjust_instant();
		raise_event(&core.operated_set, EVENT_SSET, 0, 0, setplayer, setplayer, 0);
		process_instant_event();
		if(core.current_chain.size() == 0) {
			adjust_all();
			core.hint_timing[setplayer] |= TIMING_SSET;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, FALSE);
		}
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::special_summon_rule(uint16 step, uint8 sumplayer, card * target, uint32 summon_type) {
	switch(step) {
	case 0: {
		effect_set eset;
		card* tuner = core.limit_tuner;
		group* syn = core.limit_syn;
		group* materials = core.limit_xyz;
		int32 minc = core.limit_xyz_minc;
		int32 maxc = core.limit_xyz_maxc;
		target->filter_spsummon_procedure(sumplayer, &eset, summon_type);
		target->filter_spsummon_procedure_g(sumplayer, &eset);
		core.limit_tuner = tuner;
		core.limit_syn = syn;
		core.limit_xyz = materials;
		core.limit_xyz_minc = minc;
		core.limit_xyz_maxc = maxc;
		if(!eset.size())
			return TRUE;
		core.select_effects.clear();
		core.select_options.clear();
		for(int32 i = 0; i < eset.size(); ++i) {
			core.select_effects.push_back(eset[i]);
			core.select_options.push_back(eset[i]->description);
		}
		if(core.select_options.size() == 1)
			returns.ivalue[0] = 0;
		else
			add_process(PROCESSOR_SELECT_OPTION, 0, 0, 0, sumplayer, 0);
		return FALSE;
	}
	case 1: {
		effect* peffect = core.select_effects[returns.ivalue[0]];
		core.units.begin()->peffect = peffect;
		if(peffect->code == EFFECT_SPSUMMON_PROC_G) {
			core.units.begin()->step = 19;
			return FALSE;
		}
		returns.ivalue[0] = TRUE;
		if(peffect->target) {
			pduel->lua->add_param(target, PARAM_TYPE_CARD);
			if(core.limit_tuner || core.limit_syn) {
				pduel->lua->add_param(core.limit_tuner, PARAM_TYPE_CARD);
				pduel->lua->add_param(core.limit_syn, PARAM_TYPE_GROUP);
			} else if(core.limit_xyz) {
				pduel->lua->add_param(core.limit_xyz, PARAM_TYPE_GROUP);
				if(core.limit_xyz_minc) {
					pduel->lua->add_param(core.limit_xyz_minc, PARAM_TYPE_INT);
					pduel->lua->add_param(core.limit_xyz_maxc, PARAM_TYPE_INT);
				}
			}
			core.sub_solving_event.push_back(nil_event);
			add_process(PROCESSOR_EXECUTE_TARGET, 0, peffect, 0, sumplayer, 0);
		}
		return FALSE;
	}
	case 2: {
		if(!returns.ivalue[0])
			return TRUE;
		effect_set eset;
		target->filter_effect(EFFECT_SPSUMMON_COST, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			if(eset[i]->operation) {
				core.sub_solving_event.push_back(nil_event);
				add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, sumplayer, 0);
			}
		}
		return FALSE;
	}
	case 3: {
		effect* peffect = core.units.begin()->peffect;
		target->material_cards.clear();
		target->summon_info = (peffect->get_value(target) & 0xf00ffff) | SUMMON_TYPE_SPECIAL | ((uint32)target->current.location << 16);
		if(peffect->operation) {
			pduel->lua->add_param(target, PARAM_TYPE_CARD);
			if(core.limit_tuner || core.limit_syn) {
				pduel->lua->add_param(core.limit_tuner, PARAM_TYPE_CARD);
				pduel->lua->add_param(core.limit_syn, PARAM_TYPE_GROUP);
				core.limit_tuner = 0;
				core.limit_syn = 0;
			}
			if(core.limit_xyz) {
				pduel->lua->add_param(core.limit_xyz, PARAM_TYPE_GROUP);
				core.limit_xyz = 0;
				if(core.limit_xyz_minc) {
					pduel->lua->add_param(core.limit_xyz_minc, PARAM_TYPE_INT);
					pduel->lua->add_param(core.limit_xyz_maxc, PARAM_TYPE_INT);
					core.limit_xyz_minc = 0;
					core.limit_xyz_maxc = 0;
				}
			}
			core.sub_solving_event.push_back(nil_event);
			add_process(PROCESSOR_EXECUTE_OPERATION, 0, peffect, 0, sumplayer, 0);
		}
		peffect->dec_count(sumplayer);
		return FALSE;
	}
	case 4: {
		effect* peffect = core.units.begin()->peffect;
		uint8 targetplayer = sumplayer;
		uint8 positions = POS_FACEUP;
		if(peffect->is_flag(EFFECT_FLAG_SPSUM_PARAM)) {
			positions = (uint8)peffect->s_range;
			if(peffect->o_range == 0)
				targetplayer = sumplayer;
			else
				targetplayer = 1 - sumplayer;
		}
		if(positions == 0)
			positions = POS_FACEUP_ATTACK;
		target->enable_field_effect(false);
		move_to_field(target, sumplayer, targetplayer, LOCATION_MZONE, positions);
		target->current.reason = REASON_SPSUMMON;
		target->current.reason_effect = peffect;
		target->current.reason_player = sumplayer;
		target->summon_player = sumplayer;
		set_spsummon_counter(sumplayer);
		check_card_counter(target, 3, sumplayer);
		if(target->spsummon_code)
			core.spsummon_once_map[sumplayer][target->spsummon_code]++;
		break_effect();
		return FALSE;
	}
	case 5: {
		uint8 targetplayer = target->current.controler;
		if(target->owner != targetplayer)
			set_control(target, targetplayer, 0, 0);
		core.phase_action = TRUE;
		target->current.reason_effect = core.units.begin()->peffect;
		core.summoning_card = target;
		pduel->write_buffer8(MSG_SPSUMMONING);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		return FALSE;
	}
	case 6: {
		effect* proc = core.units.begin()->peffect;
		int32 matreason = proc->value == SUMMON_TYPE_SYNCHRO ? REASON_SYNCHRO : proc->value == SUMMON_TYPE_XYZ ? REASON_XYZ : REASON_SPSUMMON;
		if (target->material_cards.size()) {
			for (auto mit = target->material_cards.begin(); mit != target->material_cards.end(); ++mit)
				raise_single_event(*mit, 0, EVENT_BE_PRE_MATERIAL, proc, matreason, sumplayer, sumplayer, 0);
		}
		raise_event(&target->material_cards, EVENT_BE_PRE_MATERIAL, proc, matreason, sumplayer, sumplayer, 0);
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 7: {
		if (core.current_chain.size() == 0) {
			if (target->is_affected_by_effect(EFFECT_CANNOT_DISABLE_SPSUMMON))
				core.units.begin()->step = 14;
			else
				core.units.begin()->step = 9;
			return FALSE;
		} else if (core.current_chain.size() > 1) {
			core.units.begin()->step = 14;
			return FALSE;
		} else {
			if (target->is_affected_by_effect(EFFECT_CANNOT_DISABLE_SPSUMMON))
				core.units.begin()->step = 15;
			else
				core.units.begin()->step = 10;
			core.reserved = core.units.front();
			return TRUE;
		}
		return FALSE;
	}
	case 10: {
		core.summoning_card = 0;
		target->set_status(STATUS_SUMMONING, TRUE);
		target->set_status(STATUS_SUMMON_DISABLED, FALSE);
		raise_event(target, EVENT_SPSUMMON, core.units.begin()->peffect, 0, sumplayer, sumplayer, 0);
		process_instant_event();
		add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, 0x101, TRUE);
		return FALSE;
	}
	case 11: {
		if(target->is_status(STATUS_SUMMONING)) {
			core.units.begin()->step = 14;
			return FALSE;
		}
		if(target->current.location == LOCATION_MZONE)
			send_to(target, 0, REASON_RULE, sumplayer, sumplayer, LOCATION_GRAVE, 0, 0);
		adjust_instant();
		add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		return TRUE;
	}
	case 15: {
		for(auto oeit = effects.oath.begin(); oeit != effects.oath.end(); ++oeit)
			if(oeit->second == core.units.begin()->peffect)
				oeit->second = 0;
		target->set_status(STATUS_SUMMONING, FALSE);
		target->set_status(STATUS_PROC_COMPLETE | STATUS_SPSUMMON_TURN, TRUE);
		target->enable_field_effect(true);
		if(target->is_status(STATUS_DISABLED))
			target->reset(RESET_DISABLE, RESET_EVENT);
		core.summoning_card = 0;
		return FALSE;
	}
	case 16: {
		pduel->write_buffer8(MSG_SPSUMMONED);
		adjust_instant();
		effect* proc = core.units.begin()->peffect;
		int32 matreason = proc->value == SUMMON_TYPE_SYNCHRO ? REASON_SYNCHRO : proc->value == SUMMON_TYPE_XYZ ? REASON_XYZ : REASON_SPSUMMON;
		if(target->material_cards.size()) {
			for(auto mit = target->material_cards.begin(); mit != target->material_cards.end(); ++mit)
				raise_single_event(*mit, 0, EVENT_BE_MATERIAL, proc, matreason, sumplayer, sumplayer, 0);
		}
		raise_event(&target->material_cards, EVENT_BE_MATERIAL, proc, matreason, sumplayer, sumplayer, 0);
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 17: {
		raise_single_event(target, 0, EVENT_SPSUMMON_SUCCESS, core.units.begin()->peffect, 0, sumplayer, sumplayer, 0);
		process_single_event();
		raise_event(target, EVENT_SPSUMMON_SUCCESS, core.units.begin()->peffect, 0, sumplayer, sumplayer, 0);
		process_instant_event();
		if(core.current_chain.size() == 0) {
			adjust_all();
			core.hint_timing[sumplayer] |= TIMING_SPSUMMON;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		}
		return TRUE;
	}
	case 20: {
		effect* peffect = core.units.begin()->peffect;
		core.units.begin()->ptarget = pduel->new_group();
		if(peffect->operation) {
			core.sub_solving_event.push_back(nil_event);
			pduel->lua->add_param(target, PARAM_TYPE_CARD);
			pduel->lua->add_param(core.units.begin()->ptarget, PARAM_TYPE_GROUP);
			add_process(PROCESSOR_EXECUTE_OPERATION, 0, peffect, 0, sumplayer, 0);
		}
		peffect->dec_count(sumplayer);
		return FALSE;
	}
	case 21: {
		group* pgroup = core.units.begin()->ptarget;
		for(auto cit = pgroup->container.begin(); cit != pgroup->container.end(); ) {
			card* pcard = *cit++;
			if(!(pcard->data.type & TYPE_MONSTER)
			        || (pcard->current.location == LOCATION_MZONE)
			        || check_unique_onfield(pcard, sumplayer, LOCATION_MZONE)
			        || pcard->is_affected_by_effect(EFFECT_CANNOT_SPECIAL_SUMMON)) {
			    pgroup->container.erase(pcard);
			    continue;
			}
			effect_set eset;
			pcard->filter_effect(EFFECT_SPSUMMON_COST, &eset);
			for(int32 i = 0; i < eset.size(); ++i) {
				if(eset[i]->operation) {
					core.sub_solving_event.push_back(nil_event);
					add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, sumplayer, 0);
				}
			}
		}
		return FALSE;
	}
	case 22: {
		group* pgroup = core.units.begin()->ptarget;
		if(pgroup->container.size() == 0)
			return TRUE;
		core.phase_action = TRUE;
		pgroup->it = pgroup->container.begin();
		return FALSE;
	}
	case 23: {
		effect* peffect = core.units.begin()->peffect;
		card* pcard = *core.units.begin()->ptarget->it;
		pcard->enable_field_effect(false);
		pcard->current.reason = REASON_SPSUMMON;
		pcard->current.reason_effect = peffect;
		pcard->current.reason_player = sumplayer;
		pcard->summon_player = sumplayer;
		pcard->summon_info = (peffect->get_value(pcard) & 0xff00ffff) | SUMMON_TYPE_SPECIAL | ((uint32)pcard->current.location << 16);
		check_card_counter(pcard, 3, sumplayer);
		move_to_field(pcard, sumplayer, sumplayer, LOCATION_MZONE, POS_FACEUP);
		return FALSE;
	}
	case 24: {
		group* pgroup = core.units.begin()->ptarget;
		card* pcard = *pgroup->it++;
		pduel->write_buffer8(MSG_SPSUMMONING);
		pduel->write_buffer32(pcard->data.code);
		pduel->write_buffer8(pcard->current.controler);
		pduel->write_buffer8(pcard->current.location);
		pduel->write_buffer8(pcard->current.sequence);
		pduel->write_buffer8(pcard->current.position);
		if(pcard->owner != pcard->current.controler)
			set_control(pcard, pcard->current.controler, 0, 0);
		if(pgroup->it != pgroup->container.end())
			core.units.begin()->step = 22;
		return FALSE;
	}
	case 25: {
		group* pgroup = core.units.begin()->ptarget;
		set_spsummon_counter(sumplayer);
		std::set<uint32> spsummon_once_set;
		for(auto cit = pgroup->container.begin(); cit != pgroup->container.end(); ++cit) {
			if((*cit)->spsummon_code)
				spsummon_once_set.insert((*cit)->spsummon_code);
		}
		for(auto cit = spsummon_once_set.begin(); cit != spsummon_once_set.end(); ++cit)
			core.spsummon_once_map[sumplayer][*cit]++;
		card_set cset;
		for(auto cit = pgroup->container.begin(); cit != pgroup->container.end(); ++cit) {
			(*cit)->set_status(STATUS_SUMMONING, TRUE);
			if(!(*cit)->is_affected_by_effect(EFFECT_CANNOT_DISABLE_SPSUMMON)) {
				cset.insert(*cit);
			}
		}
		if(cset.size()) {
			raise_event(&cset, EVENT_SPSUMMON, core.units.begin()->peffect, 0, sumplayer, sumplayer, 0);
			process_instant_event();
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, 0x101, TRUE);
		}
		return FALSE;
	}
	case 26: {
		group* pgroup = core.units.begin()->ptarget;
		card_set cset;
		for(auto cit = pgroup->container.begin(); cit != pgroup->container.end(); ) {
			card* pcard = *cit++;
			if(!pcard->is_status(STATUS_SUMMONING)) {
				pgroup->container.erase(pcard);
				if(pcard->current.location == LOCATION_MZONE)
					cset.insert(pcard);
			}
		}
		if(cset.size()) {
			send_to(&cset, 0, REASON_RULE, sumplayer, sumplayer, LOCATION_GRAVE, 0, 0);
			adjust_instant();
		}
		if(pgroup->container.size() == 0) {
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
			return TRUE;
		}
		return FALSE;
	}
	case 27: {
		group* pgroup = core.units.begin()->ptarget;
		for(auto oeit = effects.oath.begin(); oeit != effects.oath.end(); ++oeit)
			if(oeit->second == core.units.begin()->peffect)
				oeit->second = 0;
		for(auto cit = pgroup->container.begin(); cit != pgroup->container.end(); ++cit) {
			(*cit)->set_status(STATUS_SUMMONING, FALSE);
			(*cit)->set_status(STATUS_SPSUMMON_TURN, TRUE);
			(*cit)->enable_field_effect(true);
			if((*cit)->is_status(STATUS_DISABLED))
				(*cit)->reset(RESET_DISABLE, RESET_EVENT);
		}
		return FALSE;
	}
	case 28: {
		group* pgroup = core.units.begin()->ptarget;
		pduel->write_buffer8(MSG_SPSUMMONED);
		for(auto cit = pgroup->container.begin(); cit != pgroup->container.end(); ++cit)
			raise_single_event(*cit, 0, EVENT_SPSUMMON_SUCCESS, (*cit)->current.reason_effect, 0, (*cit)->current.reason_player, (*cit)->summon_player, 0);
		process_single_event();
		raise_event(&pgroup->container, EVENT_SPSUMMON_SUCCESS, core.units.begin()->peffect, 0, sumplayer, sumplayer, 0);
		process_instant_event();
		if(core.current_chain.size() == 0) {
			adjust_all();
			core.hint_timing[sumplayer] |= TIMING_SPSUMMON;
			add_process(PROCESSOR_POINT_EVENT, 0, 0, 0, FALSE, 0);
		}
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::special_summon_step(uint16 step, group * targets, card * target) {
	uint8 playerid = (target->operation_param >> 24) & 0xf;
	uint8 nocheck = (target->operation_param >> 16) & 0xff;
	uint8 nolimit = (target->operation_param >> 8) & 0xff;
	uint8 positions = target->operation_param & 0xff;
	switch(step) {
	case 0: {
		returns.ivalue[0] = FALSE;
		uint32 result = TRUE;
		effect_set eset;
		if(target->is_affected_by_effect(EFFECT_REVIVE_LIMIT) && !target->is_status(STATUS_PROC_COMPLETE)) {
			if((!nolimit && (target->current.location & 0x38)) || (!nocheck && !nolimit && (target->current.location & 0x3)))
				result = FALSE;
		}
		if(!result || (target->current.location == LOCATION_MZONE)
				|| check_unique_onfield(target, playerid, LOCATION_MZONE)
		        || !is_player_can_spsummon(core.reason_effect, target->summon_info & 0xff00ffff, positions, target->summon_player, playerid, target)
		        || get_useable_count(playerid, LOCATION_MZONE, target->summon_player, LOCATION_REASON_TOFIELD) <= 0
		        || (!nocheck && !(target->data.type & TYPE_MONSTER)))
			result = FALSE;
		if(result && !nocheck) {
			target->filter_effect(EFFECT_SPSUMMON_CONDITION, &eset);
			for(int32 i = 0; i < eset.size(); ++i) {
				pduel->lua->add_param(core.reason_effect, PARAM_TYPE_EFFECT);
				pduel->lua->add_param(target->summon_player, PARAM_TYPE_INT);
				pduel->lua->add_param(target->summon_info & 0xff00ffff, PARAM_TYPE_INT);
				pduel->lua->add_param(positions, PARAM_TYPE_INT);
				pduel->lua->add_param(playerid, PARAM_TYPE_INT);
				if(!eset[i]->check_value_condition(5)) {
					result = FALSE;
					break;
				}
			}
		}
		if(!result) {
			target->current.reason = target->temp.reason;
			target->current.reason_effect = target->temp.reason_effect;
			target->current.reason_player = target->temp.reason_player;
			if(targets)
				targets->container.erase(target);
			return TRUE;
		}
		eset.clear();
		target->filter_effect(EFFECT_SPSUMMON_COST, &eset);
		for(int32 i = 0; i < eset.size(); ++i) {
			if(eset[i]->operation) {
				core.sub_solving_event.push_back(nil_event);
				add_process(PROCESSOR_EXECUTE_OPERATION, 0, eset[i], 0, target->summon_player, 0);
			}
		}
		return FALSE;
	}
	case 1: {
		if(!targets)
			core.special_summoning.insert(target);
		target->enable_field_effect(false);
		check_card_counter(target, 3, target->summon_player);
		move_to_field(target, target->summon_player, playerid, LOCATION_MZONE, positions);
		return FALSE;
	}
	case 2: {
		pduel->write_buffer8(MSG_SPSUMMONING);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		return FALSE;
	}
	case 3: {
		returns.ivalue[0] = TRUE;
		if(target->owner != target->current.controler)
			set_control(target, target->current.controler, 0, 0);
		target->set_status(STATUS_SPSUMMON_STEP, TRUE);
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::special_summon(uint16 step, effect * reason_effect, uint8 reason_player, group * targets) {
	switch(step) {
	case 0: {
		card_vector cvs, cvo;
		for(auto iter = targets->container.begin(); iter != targets->container.end(); ++iter) {
			auto pcard = *iter;
			if(pcard->summon_player == infos.turn_player)
				cvs.push_back(pcard);
			else
				cvo.push_back(pcard);
		}
		if(!cvs.empty()) {
			if(cvs.size() > 1)
				std::sort(cvs.begin(), cvs.end(), card::card_operation_sort);
			core.hint_timing[infos.turn_player] |= TIMING_SPSUMMON;
			for(auto cvit = cvs.begin(); cvit != cvs.end(); ++cvit)
				add_process(PROCESSOR_SPSUMMON_STEP, 0, 0, targets, 0, (ptr)(*cvit));
		}
		if(!cvo.empty()) {
			if(cvo.size() > 1)
				std::sort(cvo.begin(), cvo.end(), card::card_operation_sort);
			core.hint_timing[1 - infos.turn_player] |= TIMING_SPSUMMON;
			for(auto cvit = cvo.begin(); cvit != cvo.end(); ++cvit)
				add_process(PROCESSOR_SPSUMMON_STEP, 0, 0, targets, 0, (ptr)(*cvit));
		}
		return FALSE;
	}
	case 1: {
		if(targets->container.size() == 0) {
			returns.ivalue[0] = 0;
			core.operated_set.clear();
			pduel->delete_group(targets);
			return TRUE;
		}
		bool tp = false, ntp = false;
		std::set<uint32> spsummon_once_set[2];
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			if((*cit)->summon_player == infos.turn_player)
				tp = true;
			else
				ntp = true;
			if((*cit)->spsummon_code)
				spsummon_once_set[(*cit)->summon_player].insert((*cit)->spsummon_code);
		}
		if(tp)
			set_spsummon_counter(infos.turn_player);
		if(ntp)
			set_spsummon_counter(1 - infos.turn_player);
		for(auto cit = spsummon_once_set[0].begin(); cit != spsummon_once_set[0].end(); ++cit)
			core.spsummon_once_map[0][*cit]++;
		for(auto cit = spsummon_once_set[1].begin(); cit != spsummon_once_set[1].end(); ++cit)
			core.spsummon_once_map[1][*cit]++;
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			(*cit)->set_status(STATUS_SPSUMMON_STEP, FALSE);
			(*cit)->set_status(STATUS_SPSUMMON_TURN, TRUE);
			if((*cit)->is_position(POS_FACEUP))
				(*cit)->enable_field_effect(true);
		}
		adjust_instant();
		return FALSE;
	}
	case 2: {
		pduel->write_buffer8(MSG_SPSUMMONED);
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			if(!((*cit)->current.position & POS_FACEDOWN))
				raise_single_event(*cit, 0, EVENT_SPSUMMON_SUCCESS, (*cit)->current.reason_effect, 0, (*cit)->current.reason_player, (*cit)->summon_player, 0);
			int32 summontype = (*cit)->summon_info & 0xff000000;
			if(summontype && (*cit)->material_cards.size()) {
				int32 matreason = (summontype == SUMMON_TYPE_FUSION) ? REASON_FUSION : (summontype == SUMMON_TYPE_RITUAL) ? REASON_RITUAL : (summontype == SUMMON_TYPE_XYZ) ? REASON_XYZ : 0;
				for(auto mit = (*cit)->material_cards.begin(); mit != (*cit)->material_cards.end(); ++mit)
					raise_single_event(*mit, &targets->container, EVENT_BE_MATERIAL, (*cit)->current.reason_effect, matreason, (*cit)->current.reason_player, (*cit)->summon_player, 0);
				raise_event(&((*cit)->material_cards), EVENT_BE_MATERIAL, reason_effect, matreason, reason_player, (*cit)->summon_player, 0);
			}
		}
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 3: {
		raise_event(&targets->container, EVENT_SPSUMMON_SUCCESS, reason_effect, 0, reason_player, PLAYER_NONE, 0);
		process_instant_event();
		return FALSE;
	}
	case 4: {
		core.operated_set.clear();
		core.operated_set = targets->container;
		returns.ivalue[0] = targets->container.size();
		pduel->delete_group(targets);
		return TRUE;
	}
	}
	return TRUE;
}
// destroy: step version
// PROCESSOR_DESTROY_STEP goes here
int32 field::destroy(uint16 step, group * targets, card * target, uint8 battle) {
	if(target->current.location & (LOCATION_GRAVE | LOCATION_REMOVED)) {
		target->current.reason = target->temp.reason;
		target->current.reason_effect = target->temp.reason_effect;
		target->current.reason_player = target->temp.reason_player;
		target->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
		targets->container.erase(target);
		return TRUE;
	}
	if(targets->container.find(target) == targets->container.end())
		return TRUE;
	returns.ivalue[0] = FALSE;
	effect_set eset;
	target->filter_single_continuous_effect(EFFECT_DESTROY_REPLACE, &eset);
	if(!battle)
		for (int32 i = 0; i < eset.size(); ++i)
			add_process(PROCESSOR_OPERATION_REPLACE, 0, eset[i], targets, (ptr)target, 1);
	else
		for (int32 i = 0; i < eset.size(); ++i)
			add_process(PROCESSOR_OPERATION_REPLACE, 10, eset[i], targets, (ptr)target, 1);
	return TRUE;
}
// PROCESSOR_DESTROY goes here
int32 field::destroy(uint16 step, group * targets, effect * reason_effect, uint32 reason, uint8 reason_player) {
	switch (step) {
	case 0: {
		card_set extra;
		effect_set eset;
		card_set indestructable_set;
		std::set<effect*> indestructable_effect_set;
		for (auto cit = targets->container.begin(); cit != targets->container.end();) {
			auto rm = cit++;
			card* pcard = *rm;
			if (!(pcard->current.reason & (REASON_RULE | REASON_COST))) {
				int32 is_destructable = true;
				if (pcard->is_destructable() && pcard->is_affect_by_effect(pcard->current.reason_effect)) {
					effect* indestructable_effect = pcard->check_indestructable_by_effect(pcard->current.reason_effect, pcard->current.reason_player);
					if (indestructable_effect) {
						if(reason_player != 5)
							indestructable_effect_set.insert(indestructable_effect);
						is_destructable = false;
					}
				} else
					is_destructable = false;
				if (!is_destructable) {
					indestructable_set.insert(pcard);
					continue;
				}
			}
			eset.clear();
			pcard->filter_effect(EFFECT_INDESTRUCTABLE, &eset);
			if(eset.size()) {
				bool is_destructable = true;
				for(int32 i = 0; i < eset.size(); ++i) {
					pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
					pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
					pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
					if(eset[i]->check_value_condition(3)) {
						if(reason_player != 5)
							indestructable_effect_set.insert(eset[i]);
						is_destructable = false;
						break;
					}
				}
				if(!is_destructable) {
					indestructable_set.insert(pcard);
					continue;
				}
			}
			eset.clear();
			pcard->filter_effect(EFFECT_INDESTRUCTABLE_COUNT, &eset);
			if (eset.size()) {
				bool is_destructable = true;
				for(int32 i = 0; i < eset.size(); ++i) {
					if(eset[i]->is_flag(EFFECT_FLAG_COUNT_LIMIT)) {
						if((eset[i]->reset_count & 0xf00) == 0)
							continue;
						pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
						pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
						pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
						if(eset[i]->check_value_condition(3)) {
							eset[i]->dec_count();
							indestructable_effect_set.insert(eset[i]);
							is_destructable = false;
						}
					} else {
						pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
						pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
						pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
						int32 ct;
						if(ct = eset[i]->get_value(3)) {
							auto it = pcard->indestructable_effects.insert(std::make_pair(eset[i]->id, 0));
							if(++it.first->second <= ct) {
								indestructable_effect_set.insert(eset[i]);
								is_destructable = false;
							}
						}
					}
				}
				if(!is_destructable) {
					indestructable_set.insert(pcard);
					continue;
				}
			}
			eset.clear();
			pcard->filter_effect(EFFECT_DESTROY_SUBSTITUTE, &eset);
			if (eset.size()) {
				bool sub = false;
				for (int32 i = 0; i < eset.size(); ++i) {
					pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
					pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
					pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
					if(eset[i]->check_value_condition(3)) {
						extra.insert(eset[i]->handler);
						sub = true;
					}
				}
				if(sub) {
					pcard->current.reason = pcard->temp.reason;
					pcard->current.reason_effect = pcard->temp.reason_effect;
					pcard->current.reason_player = pcard->temp.reason_player;
					core.destroy_canceled.insert(pcard);
					targets->container.erase(pcard);
				}
			}
		}
		for (auto cit = indestructable_set.begin(); cit != indestructable_set.end(); ++cit) {
			(*cit)->current.reason = (*cit)->temp.reason;
			(*cit)->current.reason_effect = (*cit)->temp.reason_effect;
			(*cit)->current.reason_player = (*cit)->temp.reason_player;
			(*cit)->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
			targets->container.erase(*cit);
		}
		for (auto cit = extra.begin(); cit != extra.end(); ++cit) {
			card* rep = *cit;
			if(targets->container.count(rep) == 0) {
				rep->temp.reason = rep->current.reason;
				rep->temp.reason_effect = rep->current.reason_effect;
				rep->temp.reason_player = rep->current.reason_player;
				rep->current.reason = REASON_EFFECT | REASON_DESTROY | REASON_REPLACE;
				rep->current.reason_effect = 0;
				rep->current.reason_player = rep->current.controler;
				rep->operation_param = (POS_FACEUP << 24) + (((int32)rep->owner) << 16) + (LOCATION_GRAVE << 8);
				targets->container.insert(rep);
			}
		}
		for (auto eit = indestructable_effect_set.begin(); eit != indestructable_effect_set.end(); ++eit) {
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_CARD);
			pduel->write_buffer8(0);
			pduel->write_buffer32((*eit)->owner->data.code);
		}
		auto pr = effects.continuous_effect.equal_range(EFFECT_DESTROY_REPLACE);
		for (; pr.first != pr.second; ++pr.first)
			add_process(PROCESSOR_OPERATION_REPLACE, 5, pr.first->second, targets, 0, 1);
		return FALSE;
	}
	case 1: {
		for (auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			add_process(PROCESSOR_DESTROY_STEP, 0, 0, targets, (ptr) (*cit), 0);
		}
		return FALSE;
	}
	case 2: {
		for (auto cit = core.destroy_canceled.begin(); cit != core.destroy_canceled.end(); ++cit)
			(*cit)->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
		core.destroy_canceled.clear();
		return FALSE;
	}
	case 3: {
		if(!targets->container.size()) {
			returns.ivalue[0] = 0;
			core.operated_set.clear();
			pduel->delete_group(targets);
			return TRUE;
		}
		card_vector cv(targets->container.begin(), targets->container.end());
		if(cv.size() > 1)
			std::sort(cv.begin(), cv.end(), card::card_operation_sort);
		for (auto cvit = cv.begin(); cvit != cv.end(); ++cvit) {
			card* pcard = *cvit;
			if(pcard->current.location & (LOCATION_GRAVE | LOCATION_REMOVED)) {
				pcard->current.reason = pcard->temp.reason;
				pcard->current.reason_effect = pcard->temp.reason_effect;
				pcard->current.reason_player = pcard->temp.reason_player;
				targets->container.erase(pcard);
				continue;
			}
			pcard->current.reason |= REASON_DESTROY;
			core.hint_timing[pcard->current.controler] |= TIMING_DESTROY;
			raise_single_event(pcard, 0, EVENT_DESTROY, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
		}
		adjust_instant();
		process_single_event();
		raise_event(&targets->container, EVENT_DESTROY, reason_effect, reason, reason_player, 0, 0);
		process_instant_event();
		return FALSE;
	}
	case 4: {
		group* sendtargets = pduel->new_group(targets->container);
		sendtargets->is_readonly = TRUE;
		uint32 dest;
		for(auto cit = sendtargets->container.begin(); cit != sendtargets->container.end(); ++cit) {
			(*cit)->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
			dest = ((*cit)->operation_param >> 8) & 0xff;
			if(!dest)
				dest = LOCATION_GRAVE;
			if((dest == LOCATION_HAND && !(*cit)->is_capable_send_to_hand(reason_player))
			        || (dest == LOCATION_DECK && !(*cit)->is_capable_send_to_deck(reason_player))
			        || (dest == LOCATION_REMOVED && !(*cit)->is_removeable(reason_player)))
				dest = LOCATION_GRAVE;
			(*cit)->operation_param = ((*cit)->operation_param & 0xffff00ff) + (dest << 8);
		}
		auto pr = effects.continuous_effect.equal_range(EFFECT_SEND_REPLACE);
		for (; pr.first != pr.second; ++pr.first)
			add_process(PROCESSOR_OPERATION_REPLACE, 5, pr.first->second, sendtargets, 0, 0);
		add_process(PROCESSOR_SENDTO, 1, reason_effect, sendtargets, reason | REASON_DESTROY, reason_player);
		return FALSE;
	}
	case 5: {
		core.operated_set.clear();
		core.operated_set = targets->container;
		for(auto cit = core.operated_set.begin(); cit != core.operated_set.end();) {
			if((*cit)->current.reason & REASON_REPLACE)
				core.operated_set.erase(cit++);
			else
				cit++;
		}
		returns.ivalue[0] = core.operated_set.size();
		pduel->delete_group(targets);
		return TRUE;
	}
	case 10: {
		effect_set eset;
		for (auto cit = targets->container.begin(); cit != targets->container.end();) {
			auto rm = cit++;
			card* pcard = *rm;
			if (!(pcard->current.reason & REASON_RULE)) {
				if (!pcard->is_destructable()) {
					pcard->current.reason = pcard->temp.reason;
					pcard->current.reason_effect = pcard->temp.reason_effect;
					pcard->current.reason_player = pcard->temp.reason_player;
					pcard->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
					targets->container.erase(pcard);
					continue;
				}
			}
			eset.clear();
			pcard->filter_effect(EFFECT_INDESTRUCTABLE, &eset);
			if(eset.size()) {
				bool indes = false;
				for(int32 i = 0; i < eset.size(); ++i) {
					pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
					pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
					pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
					if(eset[i]->check_value_condition(3)) {
						pduel->write_buffer8(MSG_HINT);
						pduel->write_buffer8(HINT_CARD);
						pduel->write_buffer8(0);
						pduel->write_buffer32(eset[i]->owner->data.code);
						indes = true;
						break;
					}
				}
				if(indes) {
					pcard->current.reason = pcard->temp.reason;
					pcard->current.reason_effect = pcard->temp.reason_effect;
					pcard->current.reason_player = pcard->temp.reason_player;
					pcard->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
					targets->container.erase(pcard);
					continue;
				}
			}
			eset.clear();
			pcard->filter_effect(EFFECT_INDESTRUCTABLE_COUNT, &eset);
			if (eset.size()) {
				bool indes = false;
				for (int32 i = 0; i < eset.size(); ++i) {
					if(eset[i]->is_flag(EFFECT_FLAG_COUNT_LIMIT)) {
						if((eset[i]->reset_count & 0xf00) == 0)
							continue;
						pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
						pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
						pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
						if(eset[i]->check_value_condition(3)) {
							eset[i]->dec_count();
							pduel->write_buffer8(MSG_HINT);
							pduel->write_buffer8(HINT_CARD);
							pduel->write_buffer8(0);
							pduel->write_buffer32(eset[i]->owner->data.code);
							indes = true;
						}
					} else {
						pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
						pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
						pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
						int32 ct;
						if(ct = eset[i]->get_value(3)) {
							auto it = pcard->indestructable_effects.insert(std::make_pair(eset[i]->id, 0));
							if(++it.first->second <= ct) {
								pduel->write_buffer8(MSG_HINT);
								pduel->write_buffer8(HINT_CARD);
								pduel->write_buffer8(0);
								pduel->write_buffer32(eset[i]->owner->data.code);
								indes = true;
							}
						}
					}
				}
				if(indes) {
					pcard->current.reason = pcard->temp.reason;
					pcard->current.reason_effect = pcard->temp.reason_effect;
					pcard->current.reason_player = pcard->temp.reason_player;
					pcard->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
					targets->container.erase(pcard);
					continue;
				}
			}
			eset.clear();
			pcard->filter_effect(EFFECT_DESTROY_SUBSTITUTE, &eset);
			if (eset.size()) {
				bool sub = false;
				for (int32 i = 0; i < eset.size(); ++i) {
					pduel->lua->add_param(pcard->current.reason_effect, PARAM_TYPE_EFFECT);
					pduel->lua->add_param(pcard->current.reason, PARAM_TYPE_INT);
					pduel->lua->add_param(pcard->current.reason_player, PARAM_TYPE_INT);
					if(eset[i]->check_value_condition(3)) {
						core.battle_destroy_rep.insert(eset[i]->handler);
						sub = true;
					}
				}
				if(sub) {
					pcard->current.reason = pcard->temp.reason;
					pcard->current.reason_effect = pcard->temp.reason_effect;
					pcard->current.reason_player = pcard->temp.reason_player;
					core.destroy_canceled.insert(pcard);
					targets->container.erase(pcard);
				}
			}
		}
		if(targets->container.size()){
			auto pr = effects.continuous_effect.equal_range(EFFECT_DESTROY_REPLACE);
			for (; pr.first != pr.second; ++pr.first)
				add_process(PROCESSOR_OPERATION_REPLACE, 12, pr.first->second, targets, 0, 1);
		}
		return FALSE;
	}
	case 11: {
		for (auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			add_process(PROCESSOR_DESTROY_STEP, 0, 0, targets, (ptr) (*cit), TRUE);
		}
		return FALSE;
	}
	case 12: {
		for (auto cit = core.destroy_canceled.begin(); cit != core.destroy_canceled.end(); ++cit)
			(*cit)->set_status(STATUS_DESTROY_CONFIRMED, FALSE);
		core.destroy_canceled.clear();
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::release(uint16 step, group * targets, card * target) {
	if(!(target->current.location & (LOCATION_ONFIELD | LOCATION_HAND))) {
		target->current.reason = target->temp.reason;
		target->current.reason_effect = target->temp.reason_effect;
		target->current.reason_player = target->temp.reason_player;
		targets->container.erase(target);
		return TRUE;
	}
	if(targets->container.find(target) == targets->container.end())
		return TRUE;
	if(!(target->current.reason & REASON_RULE)) {
		returns.ivalue[0] = FALSE;
		effect_set eset;
		target->filter_single_continuous_effect(EFFECT_RELEASE_REPLACE, &eset);
		for (int32 i = 0; i < eset.size(); ++i)
			add_process(PROCESSOR_OPERATION_REPLACE, 0, eset[i], targets, (ptr)target, 0);
	}
	return TRUE;
}
int32 field::release(uint16 step, group * targets, effect * reason_effect, uint32 reason, uint8 reason_player) {
	switch (step) {
	case 0: {
		//card_set extra;
		for (auto cit = targets->container.begin(); cit != targets->container.end();) {
			auto rm = cit++;
			card* pcard = *rm;
			if (pcard->is_status(STATUS_SUMMONING)
			        || ((reason & REASON_SUMMON) && !pcard->is_releasable_by_summon(reason_player, pcard->current.reason_card))
			        || (!(pcard->current.reason & (REASON_RULE | REASON_SUMMON | REASON_COST))
			            && (!pcard->is_affect_by_effect(pcard->current.reason_effect) || !pcard->is_releasable_by_nonsummon(reason_player)))) {
				pcard->current.reason = pcard->temp.reason;
				pcard->current.reason_effect = pcard->temp.reason_effect;
				pcard->current.reason_player = pcard->temp.reason_player;
				targets->container.erase(rm);
				continue;
			}
		}
		if(reason & REASON_RULE)
			return FALSE;
		auto pr = effects.continuous_effect.equal_range(EFFECT_RELEASE_REPLACE);
		for (; pr.first != pr.second; ++pr.first)
			add_process(PROCESSOR_OPERATION_REPLACE, 5, pr.first->second, targets, 0, 0);
		return FALSE;
	}
	case 1: {
		for (auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			add_process(PROCESSOR_RELEASE_STEP, 0, 0, targets, (ptr) (*cit), 0);
		}
		return FALSE;
	}
	case 2: {
		if(!targets->container.size()) {
			returns.ivalue[0] = 0;
			core.operated_set.clear();
			pduel->delete_group(targets);
			return TRUE;
		}
		card_vector cv(targets->container.begin(), targets->container.end());
		if(cv.size() > 1)
			std::sort(cv.begin(), cv.end(), card::card_operation_sort);
		for (auto cvit = cv.begin(); cvit != cv.end(); ++cvit) {
			card* pcard = *cvit;
			if(!(pcard->current.location & (LOCATION_ONFIELD | LOCATION_HAND))) {
				pcard->current.reason = pcard->temp.reason;
				pcard->current.reason_effect = pcard->temp.reason_effect;
				pcard->current.reason_player = pcard->temp.reason_player;
				targets->container.erase(pcard);
				continue;
			}
			pcard->current.reason |= REASON_RELEASE;
		}
		adjust_instant();
		return FALSE;
	}
	case 3: {
		group* sendtargets = pduel->new_group(targets->container);
		sendtargets->is_readonly = TRUE;
		auto pr = effects.continuous_effect.equal_range(EFFECT_SEND_REPLACE);
		for (; pr.first != pr.second; ++pr.first)
			add_process(PROCESSOR_OPERATION_REPLACE, 5, pr.first->second, sendtargets, 0, 0);
		add_process(PROCESSOR_SENDTO, 1, reason_effect, sendtargets, reason | REASON_RELEASE, reason_player);
		return FALSE;
	}
	case 4: {
		core.operated_set.clear();
		core.operated_set = targets->container;
		returns.ivalue[0] = targets->container.size();
		pduel->delete_group(targets);
		return TRUE;
	}
	}
	return TRUE;
}
// PROCESSOR_SENDTO_STEP goes here
int32 field::send_to(uint16 step, group * targets, card * target) {
	uint8 playerid = (target->operation_param >> 16) & 0xff;
	uint8 dest = (target->operation_param >> 8) & 0xff;
	//uint8 seq = (target->operation_param) & 0xff;
	if(targets->container.find(target) == targets->container.end())
		return TRUE;
	if(target->current.location == dest && target->current.controler == playerid) {
		target->current.reason = target->temp.reason;
		target->current.reason_effect = target->temp.reason_effect;
		target->current.reason_player = target->temp.reason_player;
		targets->container.erase(target);
		return TRUE;
	}
	if(!(target->current.reason & REASON_RULE)) {
		returns.ivalue[0] = FALSE;
		effect_set eset;
		target->filter_single_continuous_effect(EFFECT_SEND_REPLACE, &eset);
		for (int32 i = 0; i < eset.size(); ++i)
			add_process(PROCESSOR_OPERATION_REPLACE, 0, eset[i], targets, (ptr)target, 0);
	}
	return TRUE;
}
// PROCESSOR_SENDTO goes here
// step 1: call PROCESSOR_SENDTO_STEP
// step 6: move cards
int32 field::send_to(uint16 step, group * targets, effect * reason_effect, uint32 reason, uint8 reason_player) {
	struct exargs {
		group* targets;
		card_set leave, discard, detach;
		bool show_decktop[2];
		card_vector cv;
		card_vector::iterator cvit;
		effect* predirect;
	} ;
	switch(step) {
	case 0: {
		uint8 dest;
		for(auto cit = targets->container.begin(); cit != targets->container.end();) {
			auto rm = cit++;
			card* pcard = *rm;
			dest = (pcard->operation_param >> 8) & 0xff;
			if(!(reason & REASON_RULE) &&
			        (pcard->is_status(STATUS_SUMMONING)
			         || (!(pcard->current.reason & REASON_COST) && !pcard->is_affect_by_effect(pcard->current.reason_effect))
			         || (dest == LOCATION_HAND && !pcard->is_capable_send_to_hand(core.reason_player))
			         || (dest == LOCATION_DECK && !pcard->is_capable_send_to_deck(core.reason_player))
			         || (dest == LOCATION_REMOVED && !pcard->is_removeable(core.reason_player))
			         || (dest == LOCATION_GRAVE && !pcard->is_capable_send_to_grave(core.reason_player))
			         || (dest == LOCATION_EXTRA && !pcard->is_capable_send_to_extra(core.reason_player)))) {
				pcard->current.reason = pcard->temp.reason;
				pcard->current.reason_player = pcard->temp.reason_player;
				pcard->current.reason_effect = pcard->temp.reason_effect;
				targets->container.erase(rm);
				continue;
			}
		}
		if(reason & REASON_RULE)
			return FALSE;
		auto pr = effects.continuous_effect.equal_range(EFFECT_SEND_REPLACE);
		for (; pr.first != pr.second; ++pr.first)
			add_process(PROCESSOR_OPERATION_REPLACE, 5, pr.first->second, targets, 0, 0);
		return FALSE;
	}
	case 1: {
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			add_process(PROCESSOR_SENDTO_STEP, 0, 0, targets, (ptr)(*cit), 0);
		}
		return FALSE;
	}
	case 2: {
		if(!targets->container.size()) {
			returns.ivalue[0] = 0;
			core.operated_set.clear();
			pduel->delete_group(targets);
			return TRUE;
		}
		card_set leave_p, destroying;
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			card* pcard = *cit;
			// REASON_RULE or (REASON_EFFECT + REASON_DESTROY) => not battle destroyed
			if((pcard->current.location == LOCATION_MZONE) && pcard->is_status(STATUS_BATTLE_DESTROYED) && !(pcard->current.reason & REASON_RULE)
					&& (pcard->current.reason & (REASON_EFFECT + REASON_DESTROY)) != (REASON_EFFECT + REASON_DESTROY) && !(pcard->current.reason & REASON_BATTLE)) {
				pcard->current.reason |= REASON_DESTROY | REASON_BATTLE;
				raise_single_event(pcard, 0, EVENT_DESTROY, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
				destroying.insert(pcard);
			}
			if((pcard->current.location & LOCATION_ONFIELD) && !pcard->is_status(STATUS_SUMMON_DISABLED)) {
				raise_single_event(pcard, 0, EVENT_LEAVE_FIELD_P, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
				leave_p.insert(pcard);
			}
			if((pcard->current.location & LOCATION_ONFIELD)) {
				if(pcard->current.position & POS_FACEUP) {
					pcard->previous.code = pcard->get_code();
					pcard->previous.code2 = pcard->get_another_code();
					pcard->previous.type = pcard->get_type();
					if(pcard->current.location & LOCATION_MZONE) {
						pcard->previous.level = pcard->get_level();
						pcard->previous.rank = pcard->get_rank();
						pcard->previous.attribute = pcard->get_attribute();
						pcard->previous.race = pcard->get_race();
						pcard->previous.attack = pcard->get_attack();
						pcard->previous.defense = pcard->get_defense();
					}
				} else {
					effect_set eset;
					pcard->filter_effect(EFFECT_ADD_CODE, &eset);
					if(pcard->data.alias && !eset.size())
						pcard->previous.code = pcard->data.alias;
					else
						pcard->previous.code = pcard->data.code;
					if(eset.size())
						pcard->previous.code2 = eset.get_last()->get_value(pcard);
					else
						pcard->previous.code2 = 0;
					pcard->previous.type = pcard->data.type;
					pcard->previous.level = pcard->data.level;
					pcard->previous.rank = pcard->data.level;
					pcard->previous.attribute = pcard->data.attribute;
					pcard->previous.race = pcard->data.race;
					pcard->previous.attack = pcard->data.attack;
					pcard->previous.defense = pcard->data.defense;
				}
				effect_set eset;
				pcard->filter_effect(EFFECT_ADD_SETCODE, &eset);
				if(eset.size())
					pcard->previous.setcode = eset.get_last()->get_value(pcard);
				else
					pcard->previous.setcode = 0;
			}
		}
		if(leave_p.size())
			raise_event(&leave_p, EVENT_LEAVE_FIELD_P, reason_effect, reason, reason_player, 0, 0);
		if(destroying.size())
			raise_event(&destroying, EVENT_DESTROY, reason_effect, reason, reason_player, 0, 0);
		process_single_event();
		process_instant_event();
		return FALSE;
	}
	case 3: {
		uint32 dest, redirect, redirect_seq, check_cb;
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit)
			(*cit)->enable_field_effect(false);
		adjust_disable_check_list();
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			card* pcard = *cit;
			dest = (pcard->operation_param >> 8) & 0xff;
			redirect = 0;
			redirect_seq = 0;
			check_cb = 0;
			if((dest & LOCATION_GRAVE) && pcard->is_affected_by_effect(EFFECT_TO_GRAVE_REDIRECT_CB))
				check_cb = 1;
			if((pcard->current.location & LOCATION_ONFIELD) && !pcard->is_status(STATUS_SUMMON_DISABLED) && !pcard->is_status(STATUS_ACTIVATE_DISABLED)) {
				redirect = pcard->leave_field_redirect(pcard->current.reason);
				redirect_seq = redirect >> 16;
				redirect &= 0xffff;
			}
			if(redirect) {
				pcard->current.reason &= ~REASON_TEMPORARY;
				pcard->current.reason |= REASON_REDIRECT;
				pcard->operation_param = (pcard->operation_param & 0xffff0000) | (redirect << 8) | redirect_seq;
				dest = redirect;
			}
			redirect = pcard->destination_redirect(dest, pcard->current.reason);
			if(redirect) {
				redirect_seq = redirect >> 16;
				redirect &= 0xffff;
			}
			if(redirect && (pcard->current.location != redirect)) {
				pcard->current.reason |= REASON_REDIRECT;
				pcard->operation_param = (pcard->operation_param & 0xffff0000) | (redirect << 8) | redirect_seq;
			}
			if(check_cb)
				pcard->operation_param = (pcard->operation_param & 0xff0fffff) | (1 << 20);
		}
		return FALSE;
	}
	case 4: {
		exargs* param = new exargs;
		core.units.begin()->ptarget = (group*)param;
		param->targets = targets;
		param->show_decktop[0] = false;
		param->show_decktop[1] = false;
		param->cv.assign(targets->container.begin(), targets->container.end());
		if(param->cv.size() > 1)
			std::sort(param->cv.begin(), param->cv.end(), card::card_operation_sort);
		if(core.global_flag & GLOBALFLAG_DECK_REVERSE_CHECK) {
			int32 d0 = player[0].list_main.size() - 1, s0 = d0;
			int32 d1 = player[1].list_main.size() - 1, s1 = d1;
			for(uint32 i = 0; i < param->cv.size(); ++i) {
				card* pcard = param->cv[i];
				if(pcard->current.location != LOCATION_DECK)
					continue;
				if((pcard->current.controler == 0) && (pcard->current.sequence == s0))
					s0--;
				if((pcard->current.controler == 1) && (pcard->current.sequence == s1))
					s1--;
			}
			if((s0 != d0) && (s0 > 0)) {
				card* ptop = player[0].list_main[s0];
				if(core.deck_reversed || (ptop->current.position == POS_FACEUP_DEFENSE)) {
					pduel->write_buffer8(MSG_DECK_TOP);
					pduel->write_buffer8(0);
					pduel->write_buffer8(d0 - s0);
					if(ptop->current.position != POS_FACEUP_DEFENSE)
						pduel->write_buffer32(ptop->data.code);
					else
						pduel->write_buffer32(ptop->data.code | 0x80000000);
				}
			}
			if((s1 != d1) && (s1 > 0)) {
				card* ptop = player[1].list_main[s1];
				if(core.deck_reversed || (ptop->current.position == POS_FACEUP_DEFENSE)) {
					pduel->write_buffer8(MSG_DECK_TOP);
					pduel->write_buffer8(1);
					pduel->write_buffer8(d1 - s1);
					if(ptop->current.position != POS_FACEUP_DEFENSE)
						pduel->write_buffer32(ptop->data.code);
					else
						pduel->write_buffer32(ptop->data.code | 0x80000000);
				}
			}
		}
		param->cvit = param->cv.begin();
		return FALSE;
	}
	case 5: {
		exargs* param = (exargs*)targets;
		if(param->cvit == param->cv.end()) {
			core.units.begin()->step = 8;
			return FALSE;
		}
		card* pcard = *param->cvit;
		param->predirect = 0;
		uint32 check_cb = (pcard->operation_param >> 20) & 0xf;
		if(check_cb)
			param->predirect = pcard->is_affected_by_effect(EFFECT_TO_GRAVE_REDIRECT_CB);
		pcard->enable_field_effect(false);
		if(pcard->data.type & TYPE_TOKEN) {
			pduel->write_buffer8(MSG_MOVE);
			pduel->write_buffer32(pcard->data.code);
			pduel->write_buffer8(pcard->current.controler);
			pduel->write_buffer8(pcard->current.location);
			pduel->write_buffer8(pcard->current.sequence);
			pduel->write_buffer8(pcard->current.position);
			pduel->write_buffer8(0);
			pduel->write_buffer8(0);
			pduel->write_buffer8(0);
			pduel->write_buffer8(0);
			pduel->write_buffer32(pcard->current.reason);
			pcard->previous.controler = pcard->current.controler;
			pcard->previous.location = pcard->current.location;
			pcard->previous.sequence = pcard->current.sequence;
			pcard->previous.position = pcard->current.position;
			pcard->current.reason &= ~REASON_TEMPORARY;
			pcard->fieldid = infos.field_id++;
			pcard->reset(RESET_LEAVE, RESET_EVENT);
			pcard->clear_relate_effect();
			remove_card(pcard);
			param->leave.insert(pcard);
			++param->cvit;
			core.units.begin()->step = 4;
			return FALSE;
		}
		if(param->predirect && get_useable_count(pcard->current.controler, LOCATION_SZONE, pcard->current.controler, LOCATION_REASON_TOFIELD) > 0)
			add_process(PROCESSOR_SELECT_EFFECTYN, 0, 0, (group*)pcard, pcard->current.controler, 0);
		else
			returns.ivalue[0] = 0;
		return FALSE;
	}
	case 6: {
		if(returns.ivalue[0])
			return FALSE;
		exargs* param = (exargs*)targets;
		card* pcard = *param->cvit;
		uint8 oloc = pcard->current.location;
		uint8 playerid = (pcard->operation_param >> 16) & 0xf;
		uint8 dest = (pcard->operation_param >> 8) & 0xff;
		uint8 seq = (pcard->operation_param) & 0xff;
		if(dest == LOCATION_GRAVE) {
			core.hint_timing[pcard->current.controler] |= TIMING_TOGRAVE;
		} else if(dest == LOCATION_HAND) {
			pcard->set_status(STATUS_PROC_COMPLETE, FALSE);
			core.hint_timing[pcard->current.controler] |= TIMING_TOHAND;
		} else if(dest == LOCATION_DECK) {
			pcard->set_status(STATUS_PROC_COMPLETE, FALSE);
			core.hint_timing[pcard->current.controler] |= TIMING_TODECK;
		} else if(dest == LOCATION_REMOVED) {
			core.hint_timing[pcard->current.controler] |= TIMING_REMOVE;
		}
		//call move_card()
		if(pcard->current.controler != playerid || pcard->current.location != dest) {
			pduel->write_buffer8(MSG_MOVE);
			pduel->write_buffer32(pcard->data.code);
			pduel->write_buffer32(pcard->get_info_location());
			if(pcard->overlay_target) {
				param->detach.insert(pcard->overlay_target);
				pcard->overlay_target->xyz_remove(pcard);
			}
			move_card(playerid, pcard, dest, seq);
			pcard->current.position = (pcard->operation_param >> 24);
			pduel->write_buffer32(pcard->get_info_location());
			pduel->write_buffer32(pcard->current.reason);
		}
		if((core.deck_reversed && pcard->current.location == LOCATION_DECK) || (pcard->current.position == POS_FACEUP_DEFENSE))
			param->show_decktop[pcard->current.controler] = true;
		pcard->set_status(STATUS_LEAVE_CONFIRMED, FALSE);
		if(pcard->status & (STATUS_SUMMON_DISABLED | STATUS_ACTIVATE_DISABLED)) {
			pcard->set_status(STATUS_SUMMON_DISABLED | STATUS_ACTIVATE_DISABLED, FALSE);
			pcard->previous.location = 0;
		} else if(oloc & LOCATION_ONFIELD) {
			pcard->reset(RESET_LEAVE, RESET_EVENT);
			param->leave.insert(pcard);
		}
		if(pcard->current.reason & REASON_DISCARD)
			param->discard.insert(pcard);
		++param->cvit;
		core.units.begin()->step = 4;
		return FALSE;
	}
	case 7: {
		exargs* param = (exargs*)targets;
		card* pcard = *param->cvit;
		uint32 flag;
		get_useable_count(pcard->current.controler, LOCATION_SZONE, pcard->current.controler, LOCATION_REASON_TOFIELD, &flag);
		flag = ((flag << 8) & 0xff00) | 0xffffe0ff;
		add_process(PROCESSOR_SELECT_PLACE, 0, 0, 0, 0x10000 + pcard->current.controler, flag);
		return FALSE;
	}
	case 8: {
		exargs* param = (exargs*)targets;
		card* pcard = *param->cvit;
		uint8 oloc = pcard->current.location;
		uint8 seq = returns.bvalue[2];
		pduel->write_buffer8(MSG_MOVE);
		pduel->write_buffer32(pcard->data.code);
		pduel->write_buffer32(pcard->get_info_location());
		if(pcard->overlay_target) {
			param->detach.insert(pcard->overlay_target);
			pcard->overlay_target->xyz_remove(pcard);
		}
		move_card(pcard->current.controler, pcard, LOCATION_SZONE, seq);
		pcard->current.position = POS_FACEUP;
		pduel->write_buffer32(pcard->get_info_location());
		pduel->write_buffer32(pcard->current.reason);
		pcard->set_status(STATUS_LEAVE_CONFIRMED, FALSE);
		if(pcard->status & (STATUS_SUMMON_DISABLED | STATUS_ACTIVATE_DISABLED)) {
			pcard->set_status(STATUS_SUMMON_DISABLED | STATUS_ACTIVATE_DISABLED, FALSE);
			pcard->previous.location = 0;
		} else if(oloc & LOCATION_ONFIELD) {
			pcard->reset(RESET_LEAVE + RESET_MSCHANGE, RESET_EVENT);
			param->leave.insert(pcard);
		}
		if(pcard->current.reason & REASON_DISCARD)
			param->discard.insert(pcard);
		if(param->predirect->operation) {
			tevent e;
			e.event_cards = targets;
			e.event_player = pcard->current.controler;
			e.event_value = 0;
			e.reason = pcard->current.reason;
			e.reason_effect = reason_effect;
			e.reason_player = pcard->current.controler;
			core.sub_solving_event.push_back(e);
			add_process(PROCESSOR_EXECUTE_OPERATION, 0, param->predirect, 0, pcard->current.controler, 0);
		}
		++param->cvit;
		core.units.begin()->step = 4;
		return FALSE;
	}
	case 9: {
		exargs* param = (exargs*)targets;
		if(core.global_flag & GLOBALFLAG_DECK_REVERSE_CHECK) {
			if(param->show_decktop[0]) {
				card* ptop = *player[0].list_main.rbegin();
				pduel->write_buffer8(MSG_DECK_TOP);
				pduel->write_buffer8(0);
				pduel->write_buffer8(0);
				if(ptop->current.position != POS_FACEUP_DEFENSE)
					pduel->write_buffer32(ptop->data.code);
				else
					pduel->write_buffer32(ptop->data.code | 0x80000000);
			}
			if(param->show_decktop[1]) {
				card* ptop = *player[1].list_main.rbegin();
				pduel->write_buffer8(MSG_DECK_TOP);
				pduel->write_buffer8(1);
				pduel->write_buffer8(0);
				if(ptop->current.position != POS_FACEUP_DEFENSE)
					pduel->write_buffer32(ptop->data.code);
				else
					pduel->write_buffer32(ptop->data.code | 0x80000000);
			}
		}
		for(auto iter = param->leave.begin(); iter != param->leave.end(); ++iter)
			raise_single_event(*iter, 0, EVENT_LEAVE_FIELD, (*iter)->current.reason_effect, (*iter)->current.reason, (*iter)->current.reason_player, 0, 0);
		for(auto iter = param->discard.begin(); iter != param->discard.end(); ++iter)
			raise_single_event(*iter, 0, EVENT_DISCARD, (*iter)->current.reason_effect, (*iter)->current.reason, (*iter)->current.reason_player, 0, 0);
		if((core.global_flag & GLOBALFLAG_DETACH_EVENT) && param->detach.size()) {
			for(auto iter = param->detach.begin(); iter != param->detach.end(); ++iter) {
				if((*iter)->current.location & LOCATION_MZONE)
					raise_single_event(*iter, 0, EVENT_DETACH_MATERIAL, reason_effect, reason, reason_player, 0, 0);
			}
		}
		process_single_event();
		if(param->leave.size())
			raise_event(&param->leave, EVENT_LEAVE_FIELD, reason_effect, reason, reason_player, 0, 0);
		if(param->discard.size())
			raise_event(&param->discard, EVENT_DISCARD, reason_effect, reason, reason_player, 0, 0);
		if((core.global_flag & GLOBALFLAG_DETACH_EVENT) && param->detach.size())
			raise_event(&param->detach, EVENT_DETACH_MATERIAL, reason_effect, reason, reason_player, 0, 0);
		process_instant_event();
		adjust_instant();
		return FALSE;
	}
	case 10: {
		exargs* param = (exargs*)targets;
		core.units.begin()->ptarget = param->targets;
		targets = param->targets;
		delete param;
		uint8 nloc;
		card_set tohand, todeck, tograve, remove, released, destroyed, retgrave;
		card_set equipings, overlays;
		for(auto cit = targets->container.begin(); cit != targets->container.end(); ++cit) {
			card* pcard = *cit;
			nloc = pcard->current.location;
			if(pcard->equiping_target)
				pcard->unequip();
			if(pcard->equiping_cards.size()) {
				for(auto csit = pcard->equiping_cards.begin(); csit != pcard->equiping_cards.end();) {
					card* equipc = *(csit++);
					equipc->unequip();
					if(equipc->current.location == LOCATION_SZONE)
						equipings.insert(equipc);
				}
			}
			if(!(pcard->data.type & TYPE_TOKEN)) {
				pcard->enable_field_effect(true);
				if(nloc == LOCATION_HAND) {
					tohand.insert(pcard);
					pcard->reset(RESET_TOHAND, RESET_EVENT);
					raise_single_event(pcard, 0, EVENT_TO_HAND, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
				} else if(nloc == LOCATION_DECK || nloc == LOCATION_EXTRA) {
					todeck.insert(pcard);
					pcard->reset(RESET_TODECK, RESET_EVENT);
					raise_single_event(pcard, 0, EVENT_TO_DECK, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
				} else if(nloc == LOCATION_GRAVE) {
					pcard->reset(RESET_TOGRAVE, RESET_EVENT);
					if(pcard->current.reason & REASON_RETURN) {
						retgrave.insert(pcard);
						raise_single_event(pcard, 0, EVENT_RETURN_TO_GRAVE, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
					} else {
						tograve.insert(pcard);					
						raise_single_event(pcard, 0, EVENT_TO_GRAVE, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
					}
				}
			}
			if(nloc == LOCATION_REMOVED || ((pcard->data.type & TYPE_TOKEN) && ((pcard->operation_param >> 8) & 0xff) == LOCATION_REMOVED)) {
				remove.insert(pcard);
				if(pcard->current.reason & REASON_TEMPORARY)
					pcard->reset(RESET_TEMP_REMOVE, RESET_EVENT);
				else
					pcard->reset(RESET_REMOVE, RESET_EVENT);
				raise_single_event(pcard, 0, EVENT_REMOVE, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			}
			if(pcard->current.reason & REASON_RELEASE) {
				released.insert(pcard);
				raise_single_event(pcard, 0, EVENT_RELEASE, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			}
			// non-battle destroy
			if(pcard->current.reason & REASON_DESTROY && !(pcard->current.reason & REASON_BATTLE)) {
				destroyed.insert(pcard);
				raise_single_event(pcard, 0, EVENT_DESTROYED, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			}
			if(pcard->xyz_materials.size()) {
				for(auto clit = pcard->xyz_materials.begin(); clit != pcard->xyz_materials.end(); ++clit)
					overlays.insert(*clit);
			}
		}
		if(tohand.size())
			raise_event(&tohand, EVENT_TO_HAND, reason_effect, reason, reason_player, 0, 0);
		if(todeck.size())
			raise_event(&todeck, EVENT_TO_DECK, reason_effect, reason, reason_player, 0, 0);
		if(tograve.size())
			raise_event(&tograve, EVENT_TO_GRAVE, reason_effect, reason, reason_player, 0, 0);
		if(remove.size())
			raise_event(&remove, EVENT_REMOVE, reason_effect, reason, reason_player, 0, 0);
		if(released.size())
			raise_event(&released, EVENT_RELEASE, reason_effect, reason, reason_player, 0, 0);
		if(destroyed.size())
			raise_event(&destroyed, EVENT_DESTROYED, reason_effect, reason, reason_player, 0, 0);
		if(retgrave.size())
			raise_event(&retgrave, EVENT_RETURN_TO_GRAVE, reason_effect, reason, reason_player, 0, 0);
		process_single_event();
		process_instant_event();
		if(equipings.size())
			destroy(&equipings, 0, REASON_RULE + REASON_LOST_TARGET, PLAYER_NONE);
		if(overlays.size())
			send_to(&overlays, 0, REASON_RULE + REASON_LOST_TARGET, PLAYER_NONE, PLAYER_NONE, LOCATION_GRAVE, 0, POS_FACEUP);
		adjust_instant();
		return FALSE;
	}
	case 11: {
		core.operated_set.clear();
		core.operated_set = targets->container;
		returns.ivalue[0] = targets->container.size();
		pduel->delete_group(targets);
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::discard_deck(uint16 step, uint8 playerid, uint8 count, uint32 reason) {
	switch(step) {
	case 0: {
		if(is_player_affected_by_effect(playerid, EFFECT_CANNOT_DISCARD_DECK)) {
			core.operated_set.clear();
			returns.ivalue[0] = 0;
			return TRUE;
		}
		uint32 redirect, dest;
		int32 i = 0;
		for(auto cit = player[playerid].list_main.rbegin(); i < count && cit != player[playerid].list_main.rend(); ++cit, ++i) {
			dest = LOCATION_GRAVE;
			(*cit)->operation_param = LOCATION_GRAVE;
			(*cit)->current.reason_effect = core.reason_effect;
			(*cit)->current.reason_player = core.reason_player;
			(*cit)->current.reason = reason;
			redirect = (*cit)->destination_redirect(dest, reason) & 0xffff;
			if(redirect) {
				(*cit)->operation_param = redirect;
				dest = redirect;
			}
		}
		if(core.global_flag & GLOBALFLAG_DECK_REVERSE_CHECK) {
			if(player[playerid].list_main.size() > count) {
				card* ptop = *(player[playerid].list_main.rbegin() + count);
				if(core.deck_reversed || (ptop->current.position == POS_FACEUP_DEFENSE)) {
					pduel->write_buffer8(MSG_DECK_TOP);
					pduel->write_buffer8(playerid);
					pduel->write_buffer8(count);
					if(ptop->current.position != POS_FACEUP_DEFENSE)
						pduel->write_buffer32(ptop->data.code);
					else
						pduel->write_buffer32(ptop->data.code | 0x80000000);
				}
			}
		}
		return FALSE;
	}
	case 1: {
		uint8 dest;
		card* pcard;
		card_set tohand, todeck, tograve, remove;
		core.discarded_set.clear();
		for (int32 i = 0; i < count; ++i) {
			if(player[playerid].list_main.size() == 0)
				break;
			pcard = player[playerid].list_main.back();
			dest = pcard->operation_param;
			if(dest == LOCATION_GRAVE)
				pcard->reset(RESET_TOGRAVE, RESET_EVENT);
			else if(dest == LOCATION_HAND) {
				pcard->reset(RESET_TOHAND, RESET_EVENT);
				pcard->set_status(STATUS_PROC_COMPLETE, FALSE);
			} else if(dest == LOCATION_DECK) {
				pcard->reset(RESET_TODECK, RESET_EVENT);
				pcard->set_status(STATUS_PROC_COMPLETE, FALSE);
			} else if(dest == LOCATION_REMOVED) {
				if(pcard->current.reason & REASON_TEMPORARY)
					pcard->reset(RESET_TEMP_REMOVE, RESET_EVENT);
				else
					pcard->reset(RESET_REMOVE, RESET_EVENT);
			}
			pduel->write_buffer8(MSG_MOVE);
			pduel->write_buffer32(pcard->data.code);
			pduel->write_buffer8(pcard->current.controler);
			pduel->write_buffer8(pcard->current.location);
			pduel->write_buffer8(pcard->current.sequence);
			pduel->write_buffer8(pcard->current.position);
			pcard->enable_field_effect(false);
			pcard->cancel_field_effect();
			player[playerid].list_main.pop_back();
			pcard->previous.controler = pcard->current.controler;
			pcard->previous.location = pcard->current.location;
			pcard->previous.sequence = pcard->current.sequence;
			pcard->previous.position = pcard->current.position;
			pcard->current.controler = PLAYER_NONE;
			pcard->current.location = 0;
			add_card(pcard->owner, pcard, dest, 0);
			pcard->enable_field_effect(true);
			pcard->current.position = POS_FACEUP;
			pduel->write_buffer8(pcard->current.controler);
			pduel->write_buffer8(pcard->current.location);
			pduel->write_buffer8(pcard->current.sequence);
			pduel->write_buffer8(pcard->current.position);
			pduel->write_buffer32(pcard->current.reason);
			if(dest == LOCATION_HAND) {
				tohand.insert(pcard);
				raise_single_event(pcard, 0, EVENT_TO_HAND, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			} else if(dest == LOCATION_DECK || dest == LOCATION_EXTRA) {
				todeck.insert(pcard);
				raise_single_event(pcard, 0, EVENT_TO_DECK, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			} else if(dest == LOCATION_GRAVE) {
				tograve.insert(pcard);
				raise_single_event(pcard, 0, EVENT_TO_GRAVE, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			} else if(dest == LOCATION_REMOVED) {
				remove.insert(pcard);
				raise_single_event(pcard, 0, EVENT_REMOVE, pcard->current.reason_effect, pcard->current.reason, pcard->current.reason_player, 0, 0);
			}
			core.discarded_set.insert(pcard);
		}
		if(tohand.size())
			raise_event(&tohand, EVENT_TO_HAND, core.reason_effect, reason, core.reason_player, 0, 0);
		if(todeck.size())
			raise_event(&todeck, EVENT_TO_DECK, core.reason_effect, reason, core.reason_player, 0, 0);
		if(tograve.size())
			raise_event(&tograve, EVENT_TO_GRAVE, core.reason_effect, reason, core.reason_player, 0, 0);
		if(remove.size())
			raise_event(&remove, EVENT_REMOVE, core.reason_effect, reason, core.reason_player, 0, 0);
		process_single_event();
		process_instant_event();
		adjust_instant();
		return FALSE;
	}
	case 2: {
		core.operated_set.clear();
		core.operated_set = core.discarded_set;
		returns.ivalue[0] = core.discarded_set.size();
		core.discarded_set.clear();
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::move_to_field(uint16 step, card * target, uint32 enable, uint32 ret, uint32 is_equip) {
	uint32 move_player = (target->operation_param >> 24) & 0xff;
	uint32 playerid = (target->operation_param >> 16) & 0xff;
	uint32 location = (target->operation_param >> 8) & 0xff;
	uint32 positions = (target->operation_param) & 0xff;
	switch(step) {
	case 0: {
		returns.ivalue[0] = FALSE;
		if((ret == 1) && (!(target->current.reason & REASON_TEMPORARY) || (target->current.reason_effect->owner != core.reason_effect->owner)))
			return TRUE;
		if(!is_equip && location == LOCATION_SZONE && (target->data.type & TYPE_FIELD) && (target->data.type & TYPE_SPELL)) {
			card* pcard = get_field_card(playerid, LOCATION_SZONE, 5);
			if(pcard) {
				if(core.duel_options & DUEL_OBSOLETE_RULING)
					destroy(pcard, 0, REASON_RULE, pcard->current.controler);
				else // new ruling
					send_to(pcard, 0, REASON_RULE, pcard->current.controler, PLAYER_NONE, LOCATION_GRAVE, 0, 0);
				adjust_all();
			}
		} else if(!is_equip && location == LOCATION_SZONE && (target->data.type & TYPE_PENDULUM)) {
			uint32 flag = 0;
			if(is_location_useable(playerid, LOCATION_SZONE, 6))
				flag |= 1 << 14;
			if(is_location_useable(playerid, LOCATION_SZONE, 7))
				flag |= 1 << 15;
			if(move_player != playerid)
				flag = flag << 16;
			pduel->write_buffer8(MSG_SELECT_PLACE);
			pduel->write_buffer8(move_player);
			pduel->write_buffer8(1);
			pduel->write_buffer32(~flag);
		} else {
			uint32 flag;
			uint32 lreason = (target->current.location == LOCATION_MZONE) ? LOCATION_REASON_CONTROL : LOCATION_REASON_TOFIELD;
			uint32 ct = get_useable_count(playerid, location, move_player, lreason, &flag);
			if((ret == 1) && (ct <= 0 || target->is_status(STATUS_FORBIDDEN))) {
				core.units.begin()->step = 3;
				send_to(target, core.reason_effect, REASON_EFFECT, core.reason_player, PLAYER_NONE, LOCATION_GRAVE, 0, 0);
				return FALSE;
			}
			if(!ct)
				return TRUE;
			if(ret && is_location_useable(playerid, location, target->previous.sequence)) {
				returns.bvalue[2] = target->previous.sequence;
				return FALSE;
			}
			if(move_player == playerid) {
				if(location == LOCATION_SZONE)
					flag = ((flag << 8) & 0xff00) | 0xffff00ff;
				else
					flag = (flag & 0xff) | 0xffffff00;
			} else {
				if(location == LOCATION_SZONE)
					flag = ((flag << 24) & 0xff000000) | 0xffffff;
				else
					flag = ((flag << 16) & 0xff0000) | 0xff00ffff;
			}
			flag |= 0xe0e0e0e0;
			add_process(PROCESSOR_SELECT_PLACE, 0, 0, 0, 0x10000 + move_player, flag);
		}
		return FALSE;
	}
	case 1: {
		uint32 seq = returns.bvalue[2];
		if(!is_equip && location == LOCATION_SZONE && (target->data.type & TYPE_FIELD) && (target->data.type & TYPE_SPELL))
			seq = 5;
		if(ret != 1) {
			if(location != target->current.location) {
				uint32 resetflag = 0;
				if(location & LOCATION_ONFIELD)
					resetflag |= RESET_TOFIELD;
				if(target->current.location & LOCATION_ONFIELD)
					resetflag |= RESET_LEAVE;
				effect* peffect = target->is_affected_by_effect(EFFECT_PRE_MONSTER);
				if((location & LOCATION_ONFIELD) && (target->current.location & LOCATION_ONFIELD) && !(peffect && (peffect->value & TYPE_TRAP)))
					resetflag |= RESET_MSCHANGE;
				target->reset(resetflag, RESET_EVENT);
			}
			if(!(target->current.location & LOCATION_ONFIELD))
				target->relate_effect.clear();
		} else {
			if(target->turnid != infos.turn_id) {
				target->set_status(STATUS_SUMMON_TURN, FALSE);
				target->set_status(STATUS_FLIP_SUMMON_TURN, FALSE);
				target->set_status(STATUS_SPSUMMON_TURN, FALSE);
				target->set_status(STATUS_SET_TURN, FALSE);
				target->set_status(STATUS_FORM_CHANGED, FALSE);
			}
		}
		target->temp.sequence = seq;
		if(location != LOCATION_MZONE) {
			returns.ivalue[0] = positions;
			return FALSE;
		}
		add_process(PROCESSOR_SELECT_POSITION, 0, 0, 0, (positions << 16) + move_player, target->data.code);
		return FALSE;
	}
	case 2: {
		if(core.global_flag & GLOBALFLAG_DECK_REVERSE_CHECK) {
			if(target->current.location == LOCATION_DECK) {
				uint32 curp = target->current.controler;
				uint32 curs = target->current.sequence;
				if(curs > 0 && (curs == player[curp].list_main.size() - 1)) {
					card* ptop = player[curp].list_main[curs - 1];
					if(core.deck_reversed || (ptop->current.position == POS_FACEUP_DEFENSE)) {
						pduel->write_buffer8(MSG_DECK_TOP);
						pduel->write_buffer8(curp);
						pduel->write_buffer8(1);
						if(ptop->current.position != POS_FACEUP_DEFENSE)
							pduel->write_buffer32(ptop->data.code);
						else
							pduel->write_buffer32(ptop->data.code | 0x80000000);
					}
				}
			}
		}
		pduel->write_buffer8(MSG_MOVE);
		pduel->write_buffer32(target->data.code);
		pduel->write_buffer32(target->get_info_location());
		if(target->overlay_target)
			target->overlay_target->xyz_remove(target);
		move_card(playerid, target, location, target->temp.sequence);
		target->current.position = returns.ivalue[0];
		if((target->previous.location & LOCATION_ONFIELD) && (location & LOCATION_ONFIELD))
			target->set_status(STATUS_LEAVE_CONFIRMED, FALSE);
		else
			target->set_status(STATUS_LEAVE_CONFIRMED | STATUS_ACTIVATED, FALSE);
		pduel->write_buffer32(target->get_info_location());
		pduel->write_buffer32(target->current.reason);
		if((target->current.location != LOCATION_MZONE)) {
			if(target->equiping_cards.size()) {
				destroy(&target->equiping_cards, 0, REASON_LOST_TARGET + REASON_RULE, PLAYER_NONE);
				for(auto csit = target->equiping_cards.begin(); csit != target->equiping_cards.end();) {
					auto rm = csit++;
					(*rm)->unequip();
				}
			}
			if(target->xyz_materials.size()) {
				card_set overlays;
				overlays.insert(target->xyz_materials.begin(), target->xyz_materials.end());
				send_to(&overlays, 0, REASON_LOST_TARGET + REASON_RULE, PLAYER_NONE, PLAYER_NONE, LOCATION_GRAVE, 0, POS_FACEUP);
			}
		}
		if((target->previous.location == LOCATION_SZONE) && target->equiping_target)
			target->unequip();
		if(enable || ((ret == 1) && target->is_position(POS_FACEUP)))
			target->enable_field_effect(true);
		if(ret == 1 && target->current.location == LOCATION_MZONE && !(target->data.type & TYPE_MONSTER))
			send_to(target, 0, REASON_RULE, PLAYER_NONE, PLAYER_NONE, LOCATION_GRAVE, 0, 0);
		adjust_disable_check_list();
		return FALSE;
	}
	case 3: {
		returns.ivalue[0] = TRUE;
		return TRUE;
	}
	case 4: {
		returns.ivalue[0] = FALSE;
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::change_position(uint16 step, group * targets, effect * reason_effect, uint8 reason_player, uint32 enable) {
	switch(step) {
	case 0: {
		card_set equipings;
		card_set flips;
		card_set ssets;
		card_set pos_changed;
		card_vector cv(targets->container.begin(), targets->container.end());
		if(cv.size() > 1)
			std::sort(cv.begin(), cv.end(), card::card_operation_sort);
		for(auto cvit = cv.begin(); cvit != cv.end(); ++cvit) {
			card* pcard = *cvit;
			uint8 npos = pcard->operation_param & 0xff;
			uint8 opos = pcard->current.position;
			uint8 flag = pcard->operation_param >> 16;
			if(pcard->is_status(STATUS_SUMMONING) || pcard->overlay_target || !(pcard->current.location & LOCATION_ONFIELD)
			        || !pcard->is_affect_by_effect(reason_effect) || npos == opos
			        || (!(pcard->data.type & TYPE_TOKEN) && (opos & POS_FACEUP) &&  (npos & POS_FACEDOWN) && !pcard->is_capable_turn_set(reason_player))
			        || (reason_effect && pcard->is_affected_by_effect(EFFECT_CANNOT_CHANGE_POS_E))) {
				targets->container.erase(pcard);
			} else {
				if((pcard->data.type & TYPE_TOKEN) && (npos & POS_FACEDOWN))
					npos = POS_FACEUP_DEFENSE;
				pcard->previous.position = opos;
				pcard->current.position = npos;
				if(npos & POS_DEFENSE)
					pcard->set_status(STATUS_ATTACK_CANCELED, TRUE);
				pcard->set_status(STATUS_JUST_POS, TRUE);
				pduel->write_buffer8(MSG_POS_CHANGE);
				pduel->write_buffer32(pcard->data.code);
				pduel->write_buffer8(pcard->current.controler);
				pduel->write_buffer8(pcard->current.location);
				pduel->write_buffer8(pcard->current.sequence);
				pduel->write_buffer8(pcard->previous.position);
				pduel->write_buffer8(pcard->current.position);
				core.hint_timing[pcard->current.controler] |= TIMING_POS_CHANGE;
				if((opos & POS_FACEDOWN) && (npos & POS_FACEUP)) {
					pcard->fieldid = infos.field_id++;
					if(pcard->current.location == LOCATION_MZONE) {
						raise_single_event(pcard, 0, EVENT_FLIP, reason_effect, 0, reason_player, 0, flag);
						flips.insert(pcard);
					}
					if(enable) {
						if(!reason_effect || !(reason_effect->type & 0x7f0) || pcard->current.location != LOCATION_MZONE)
							pcard->enable_field_effect(true);
						else
							core.delayed_enable_set.insert(pcard);
					} else
						pcard->refresh_disable_status();
				}
				if(pcard->current.location == LOCATION_MZONE) {
					raise_single_event(pcard, 0, EVENT_CHANGE_POS, reason_effect, 0, reason_player, 0, 0);
					pos_changed.insert(pcard);
				}
				bool trapmonster = false;
				if((opos & POS_FACEUP) && (npos & POS_FACEDOWN)) {
					if(pcard->get_type() & TYPE_TRAPMONSTER)
						trapmonster = true;
					pcard->reset(RESET_TURN_SET, RESET_EVENT);
					pcard->set_status(STATUS_SET_TURN, TRUE);
					pcard->enable_field_effect(false);
					pcard->summon_info &= 0xdf00ffff;
					if((pcard->summon_info & SUMMON_TYPE_PENDULUM) == SUMMON_TYPE_PENDULUM)
						pcard->summon_info &= 0xf000ffff;
					pcard->spsummon_counter[0] = pcard->spsummon_counter[1] = 0;
					pcard->spsummon_counter_rst[0] = pcard->spsummon_counter_rst[1] = 0;
				}
				if((npos & POS_FACEDOWN) && pcard->equiping_cards.size()) {
					for(auto csit = pcard->equiping_cards.begin(); csit != pcard->equiping_cards.end();) {
						auto erm = csit++;
						equipings.insert(*erm);
						(*erm)->unequip();
					}
				}
				if((npos & POS_FACEDOWN) && pcard->equiping_target)
					pcard->unequip();
				if(trapmonster) {
					refresh_location_info_instant();
					move_to_field(pcard, pcard->current.controler, pcard->current.controler, LOCATION_SZONE, POS_FACEDOWN, FALSE, 2);
					raise_single_event(pcard, 0, EVENT_SSET, reason_effect, 0, reason_player, 0, 0);
					ssets.insert(pcard);
				}
			}
		}
		adjust_instant();
		process_single_event();
		if(flips.size())
			raise_event(&flips, EVENT_FLIP, reason_effect, 0, reason_player, 0, 0);
		if(ssets.size())
			raise_event(&ssets, EVENT_SSET, reason_effect, 0, reason_player, 0, 0);
		if(pos_changed.size())
			raise_event(&pos_changed, EVENT_CHANGE_POS, reason_effect, 0, reason_player, 0, 0);
		process_instant_event();
		if(equipings.size())
			destroy(&equipings, 0, REASON_LOST_TARGET + REASON_RULE, PLAYER_NONE);
		return FALSE;
	}
	case 1: {
		core.operated_set.clear();
		core.operated_set = targets->container;
		returns.ivalue[0] = targets->container.size();
		pduel->delete_group(targets);
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::operation_replace(uint16 step, effect * replace_effect, group * targets, ptr arg1, ptr arg2) {
	switch (step) {
	case 0: {
		if(returns.ivalue[0])
			return TRUE;
		if(!replace_effect->target)
			return TRUE;
		card* target = (card*)arg1;
		tevent e;
		e.event_cards = targets;
		e.event_player = replace_effect->get_handler_player();
		e.event_value = 0;
		e.reason = target->current.reason;
		e.reason_effect = target->current.reason_effect;
		e.reason_player = target->current.reason_player;
		if(!replace_effect->is_activateable(replace_effect->get_handler_player(), e))
			return TRUE;
		chain newchain;
		newchain.chain_id = 0;
		newchain.chain_count = 0;
		newchain.triggering_effect = replace_effect;
		newchain.triggering_player = e.event_player;
		newchain.evt = e;
		newchain.target_cards = 0;
		newchain.target_player = PLAYER_NONE;
		newchain.target_param = 0;
		newchain.disable_player = PLAYER_NONE;
		newchain.disable_reason = 0;
		newchain.flag = 0;
		core.solving_event.push_front(e);
		core.sub_solving_event.push_back(e);
		core.continuous_chain.push_back(newchain);
		add_process(PROCESSOR_EXECUTE_TARGET, 0, replace_effect, 0, replace_effect->get_handler_player(), 0);
		return FALSE;
	}
	case 1: {
		card* target = (card*)arg1;
		if (returns.ivalue[0]) {
			targets->container.erase(target);
			target->current.reason = target->temp.reason;
			target->current.reason_effect = target->temp.reason_effect;
			target->current.reason_player = target->temp.reason_player;
			if(arg2)
				core.destroy_canceled.insert(target);
			replace_effect->dec_count();
		} else
			core.units.begin()->step = 2;
		return FALSE;
	}
	case 2: {
		if(!replace_effect->operation)
			return FALSE;
		core.sub_solving_event.push_back(*core.solving_event.begin());
		add_process(PROCESSOR_EXECUTE_OPERATION, 0, replace_effect, 0, replace_effect->get_handler_player(), 0);
		return FALSE;
	}
	case 3: {
		if(core.continuous_chain.rbegin()->target_cards)
			pduel->delete_group(core.continuous_chain.rbegin()->target_cards);
		for(auto oit = core.continuous_chain.rbegin()->opinfos.begin(); oit != core.continuous_chain.rbegin()->opinfos.end(); ++oit) {
			if(oit->second.op_cards)
				pduel->delete_group(oit->second.op_cards);
		}
		core.continuous_chain.pop_back();
		core.solving_event.pop_front();
		return TRUE;
	}
	case 5: {
		tevent e;
		e.event_cards = targets;
		e.event_player = replace_effect->get_handler_player();
		e.event_value = 0;
		if(targets->container.size() == 0)
			return TRUE;
		card* pc = *targets->container.begin();
		e.reason = pc->current.reason;
		e.reason_effect = pc->current.reason_effect;
		e.reason_player = pc->current.reason_player;
		if(!replace_effect->is_activateable(replace_effect->get_handler_player(), e) || !replace_effect->value)
			return TRUE;
		chain newchain;
		newchain.chain_id = 0;
		newchain.chain_count = 0;
		newchain.triggering_effect = replace_effect;
		newchain.triggering_player = e.event_player;
		newchain.evt = e;
		newchain.target_cards = 0;
		newchain.target_player = PLAYER_NONE;
		newchain.target_param = 0;
		newchain.disable_player = PLAYER_NONE;
		newchain.disable_reason = 0;
		newchain.flag = 0;
		core.solving_event.push_front(e);
		core.sub_solving_event.push_back(e);
		core.continuous_chain.push_back(newchain);
		add_process(PROCESSOR_EXECUTE_TARGET, 0, replace_effect, 0, replace_effect->get_handler_player(), 0);
		return FALSE;
	}
	case 6: {
		if (returns.ivalue[0]) {
			uint32 is_destroy = arg2;
			for (auto cit = targets->container.begin(); cit != targets->container.end();) {
				auto rm = cit++;
				if (replace_effect->get_value(*rm)) {
					(*rm)->current.reason = (*rm)->temp.reason;
					(*rm)->current.reason_effect = (*rm)->temp.reason_effect;
					(*rm)->current.reason_player = (*rm)->temp.reason_player;
					if(is_destroy)
						core.destroy_canceled.insert(*rm);
					targets->container.erase(rm);
				}
			}
			replace_effect->dec_count();
		} else
			core.units.begin()->step = 7;
		return FALSE;
	}
	case 7: {
		if(!replace_effect->operation)
			return FALSE;
		core.sub_solving_event.push_back(*core.solving_event.begin());
		add_process(PROCESSOR_EXECUTE_OPERATION, 0, replace_effect, 0, replace_effect->get_handler_player(), 0);
		return FALSE;
	}
	case 8: {
		if(core.continuous_chain.rbegin()->target_cards)
			pduel->delete_group(core.continuous_chain.rbegin()->target_cards);
		for(auto oit = core.continuous_chain.rbegin()->opinfos.begin(); oit != core.continuous_chain.rbegin()->opinfos.end(); ++oit) {
			if(oit->second.op_cards)
				pduel->delete_group(oit->second.op_cards);
		}
		core.continuous_chain.pop_back();
		core.solving_event.pop_front();
		return TRUE;
	}
	case 10: {
		if(returns.ivalue[0])
			return TRUE;
		if(!replace_effect->target)
			return TRUE;
		card* target = (card*)arg1;
		tevent e;
		e.event_cards = targets;
		e.event_player = replace_effect->get_handler_player();
		e.event_value = 0;
		e.reason = target->current.reason;
		e.reason_effect = target->current.reason_effect;
		e.reason_player = target->current.reason_player;
		if(!replace_effect->is_activateable(replace_effect->get_handler_player(), e))
			return TRUE;
		chain newchain;
		newchain.chain_id = 0;
		newchain.chain_count = 0;
		newchain.triggering_effect = replace_effect;
		newchain.triggering_player = e.event_player;
		newchain.evt = e;
		newchain.target_cards = 0;
		newchain.target_player = PLAYER_NONE;
		newchain.target_param = 0;
		newchain.disable_player = PLAYER_NONE;
		newchain.disable_reason = 0;
		newchain.flag = 0;
		core.sub_solving_event.push_back(e);
		core.continuous_chain.push_back(newchain);
		add_process(PROCESSOR_EXECUTE_TARGET, 0, replace_effect, 0, replace_effect->get_handler_player(), 0);
		return FALSE;
	}
	case 11: {
		card* target = (card*)arg1;
		if (returns.ivalue[0]) {
			targets->container.erase(target);
			target->current.reason = target->temp.reason;
			target->current.reason_effect = target->temp.reason_effect;
			target->current.reason_player = target->temp.reason_player;
			if(arg2)
				core.destroy_canceled.insert(target);
			replace_effect->dec_count();
			core.desrep_chain.push_back(core.continuous_chain.front());
		}
		core.continuous_chain.pop_front();
		return TRUE;
	}
	case 12: {
		tevent e;
		e.event_cards = targets;
		e.event_player = replace_effect->get_handler_player();
		e.event_value = 0;
		if(targets->container.size() == 0)
			return TRUE;
		card* pc = *targets->container.begin();
		e.reason = pc->current.reason;
		e.reason_effect = pc->current.reason_effect;
		e.reason_player = pc->current.reason_player;
		if(!replace_effect->is_activateable(replace_effect->get_handler_player(), e) || !replace_effect->value)
			return TRUE;
		chain newchain;
		newchain.chain_id = 0;
		newchain.chain_count = 0;
		newchain.triggering_effect = replace_effect;
		newchain.triggering_player = e.event_player;
		newchain.evt = e;
		newchain.target_cards = 0;
		newchain.target_player = PLAYER_NONE;
		newchain.target_param = 0;
		newchain.disable_player = PLAYER_NONE;
		newchain.disable_reason = 0;
		newchain.flag = 0;
		core.continuous_chain.push_back(newchain);
		core.sub_solving_event.push_back(e);
		add_process(PROCESSOR_EXECUTE_TARGET, 0, replace_effect, 0, replace_effect->get_handler_player(), 0);
		return FALSE;
	}
	case 13: {
		if (returns.ivalue[0]) {
			uint32 is_destroy = arg2;
			for (auto cit = targets->container.begin(); cit != targets->container.end();) {
				auto rm = cit++;
				if (replace_effect->get_value(*rm)) {
					(*rm)->current.reason = (*rm)->temp.reason;
					(*rm)->current.reason_effect = (*rm)->temp.reason_effect;
					(*rm)->current.reason_player = (*rm)->temp.reason_player;
					if(is_destroy)
						core.destroy_canceled.insert(*rm);
					targets->container.erase(rm);
				}
			}
			replace_effect->dec_count();
			core.desrep_chain.push_back(core.continuous_chain.front());
		}
		core.continuous_chain.pop_front();
		return TRUE;
	}
	case 15: {
		if(core.desrep_chain.size() == 0)
			return TRUE;
		core.continuous_chain.push_back(core.desrep_chain.front());
		core.desrep_chain.pop_front();
		effect* reffect = core.continuous_chain.back().triggering_effect;
		if(!reffect->operation)
			return FALSE;
		core.sub_solving_event.push_back(core.continuous_chain.back().evt);
		add_process(PROCESSOR_EXECUTE_OPERATION, 0, reffect, 0, reffect->get_handler_player(), 0);
		return FALSE;
	}
	case 16: {
		if(core.continuous_chain.rbegin()->target_cards)
			pduel->delete_group(core.continuous_chain.rbegin()->target_cards);
		for(auto oit = core.continuous_chain.rbegin()->opinfos.begin(); oit != core.continuous_chain.rbegin()->opinfos.end(); ++oit) {
			if(oit->second.op_cards)
				pduel->delete_group(oit->second.op_cards);
		}
		core.continuous_chain.pop_back();
		core.units.begin()->step = 14;
		return FALSE;
	}
	}
	return TRUE;
}
int32 field::select_synchro_material(int16 step, uint8 playerid, card* pcard, int32 min, int32 max, card* smat, group* mg) {
	switch(step) {
	case 0: {
		core.select_cards.clear();
		if(core.global_flag & GLOBALFLAG_MUST_BE_SMATERIAL) {
			effect_set eset;
			filter_player_effect(pcard->current.controler, EFFECT_MUST_BE_SMATERIAL, &eset);
			if(eset.size() && (!mg || mg->has_card(eset[0]->handler))) {
				core.select_cards.push_back(eset[0]->handler);
				pduel->restore_assumes();
				pduel->write_buffer8(MSG_HINT);
				pduel->write_buffer8(HINT_SELECTMSG);
				pduel->write_buffer8(playerid);
				pduel->write_buffer32(512);
				add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid + ((uint32)core.summon_cancelable << 16), 0x10001);
				return FALSE;
			}
		}
		if(mg) {
			for(auto cit = mg->container.begin(); cit != mg->container.end(); ++cit) {
				if(check_tuner_material(pcard, *cit, -2, -1, min, max, smat, mg))
					core.select_cards.push_back(*cit);
			}
		} else {
			for(uint8 p = 0; p < 2; ++p) {
				for(int32 i = 0; i < 5; ++i) {
					card* tuner = player[p].list_mzone[i];
					if(check_tuner_material(pcard, tuner, -2, -1, min, max, smat, mg))
						core.select_cards.push_back(tuner);
				}
			}
		}
		if(core.select_cards.size() == 0)
			return TRUE;
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(512);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid + ((uint32)core.summon_cancelable << 16), 0x10001);
		return FALSE;
	}
	case 1: {
		if(returns.ivalue[0] == -1) {
			lua_pop(pduel->lua->current_state, 2);
			pduel->lua->add_param((void*)0, PARAM_TYPE_GROUP);
			core.limit_tuner = 0;
			return TRUE;
		}
		card* tuner = core.select_cards[returns.bvalue[1]];
		effect* pcheck = tuner->is_affected_by_effect(EFFECT_SYNCHRO_CHECK);
		if(pcheck)
			pcheck->get_value(tuner);
		core.limit_tuner = tuner;
		effect* peffect;
		if((peffect = tuner->is_affected_by_effect(EFFECT_SYNCHRO_MATERIAL_CUSTOM, pcard))) {
			if(!peffect->operation)
				return FALSE;
			core.synchro_materials.clear();
			pduel->lua->add_param(pcard, PARAM_TYPE_CARD);
			pduel->lua->add_param(-1, PARAM_TYPE_INDEX);
			pduel->lua->add_param(min, PARAM_TYPE_INT);
			pduel->lua->add_param(max, PARAM_TYPE_INT);
			core.sub_solving_event.push_back(nil_event);
			add_process(PROCESSOR_EXECUTE_OPERATION, 0, peffect, 0, playerid, 0);
		} else {
			core.units.begin()->step = 2;
		}
		return FALSE;
	}
	case 2: {
		lua_pop(pduel->lua->current_state, 2);
		group* pgroup = pduel->new_group(core.synchro_materials);
		pgroup->container.insert(core.limit_tuner);
		pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
		pduel->restore_assumes();
		core.limit_tuner = 0;
		return TRUE;
	}
	case 3: {
		if(!smat)
			return FALSE;
		card* tuner = core.limit_tuner;
		effect* pcheck = tuner->is_affected_by_effect(EFFECT_SYNCHRO_CHECK);
		if(pcheck)
			pcheck->get_value(smat);
		if(min == 1) {
			int32 lv = pcard->get_level();
			card_vector nsyn;
			nsyn.push_back(tuner);
			nsyn.push_back(smat);
			tuner->operation_param = tuner->get_synchro_level(pcard);
			smat->operation_param = smat->get_synchro_level(pcard);
			if(check_with_sum_limit_m(nsyn, lv, 0, 0, 0, 2))
				core.units.begin()->step = 5;
		}
		return FALSE;
	}
	case 4: {
		int32 lv = pcard->get_level();
		int32 mcount = 1;
		card* tuner = core.limit_tuner;
		effect* pcheck = tuner->is_affected_by_effect(EFFECT_SYNCHRO_CHECK);
		core.must_select_cards.clear();
		core.must_select_cards.push_back(tuner);
		tuner->operation_param = tuner->get_synchro_level(pcard);
		if(smat) {
			min--;
			max--;
			core.must_select_cards.push_back(smat);
			smat->operation_param = smat->get_synchro_level(pcard);
			mcount++;
		}
		core.select_cards.clear();
		if(mg) {
			for(auto cit = mg->container.begin(); cit != mg->container.end(); ++cit) {
				card* pm = *cit;
				if(pm != tuner && pm != smat && pm->is_can_be_synchro_material(pcard, tuner)) {
					if(pcheck)
						pcheck->get_value(pm);
					if(pm->current.location == LOCATION_MZONE && !pm->is_position(POS_FACEUP))
						continue;
					if(!pduel->lua->check_matching(pm, -1, 0))
						continue;
					core.select_cards.push_back(pm);
					pm->operation_param = pm->get_synchro_level(pcard);
				}
			}
		} else {
			for(uint8 np = 0; np < 2; ++np) {
				for(int32 i = 0; i < 5; ++i) {
					card* pm = player[np].list_mzone[i];
					if(pm && pm != tuner && pm != smat && pm->is_position(POS_FACEUP) && pm->is_can_be_synchro_material(pcard, tuner)) {
						if(pcheck)
							pcheck->get_value(pm);
						if(!pduel->lua->check_matching(pm, -1, 0))
							continue;
						core.select_cards.push_back(pm);
						pm->operation_param = pm->get_synchro_level(pcard);
					}
				}
			}
		}
		if(core.global_flag & GLOBALFLAG_SCRAP_CHIMERA) {
			effect* peffect = 0;
			for(auto cit = core.select_cards.begin(); cit != core.select_cards.end(); ++cit) {
				peffect = (*cit)->is_affected_by_effect(EFFECT_SCRAP_CHIMERA);
				if(peffect)
					break;
			}
			if(peffect) {
				card_vector nsyn(core.must_select_cards);
				nsyn.insert(nsyn.end(), core.select_cards.begin(), core.select_cards.end());
				card_vector nsyn_filtered;
				for(auto cit = nsyn.begin(); cit != nsyn.end(); ++cit) {
					if(!peffect->get_value(*cit))
						nsyn_filtered.push_back(*cit);
				}
				if(nsyn_filtered.size() < nsyn.size()) {
					card_vector nsyn_removed;
					for(auto cit = nsyn.begin(); cit != nsyn.end(); ++cit) {
						if(!(*cit)->is_affected_by_effect(EFFECT_SCRAP_CHIMERA))
							nsyn_removed.push_back(*cit);
					}
					bool mfiltered = true;
					bool mremoved = true;
					for(int32 i = 0; i < mcount; ++i) {
						if(peffect->get_value(nsyn[i]))
							mfiltered = false;
						if(nsyn[i]->is_affected_by_effect(EFFECT_SCRAP_CHIMERA))
							mremoved = false;
					}
					if(mfiltered && check_with_sum_limit_m(nsyn_filtered, lv, 0, min, max, mcount)) {
						if(mremoved && check_with_sum_limit_m(nsyn_removed, lv, 0, min, max, mcount)) {
							add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, playerid, peffect->description);
							core.units.begin()->step = 6;
							return FALSE;
						} else
							core.select_cards.assign(nsyn_filtered.begin() + mcount, nsyn_filtered.end());
					} else
						core.select_cards.assign(nsyn_removed.begin() + mcount, nsyn_removed.end());
				}
			}
		}
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(512);
		add_process(PROCESSOR_SELECT_SUM, 0, 0, 0, lv, playerid + (min << 16) + (max << 24));
		return FALSE;
	}
	case 5: {
		lua_pop(pduel->lua->current_state, 2);
		group* pgroup = pduel->new_group();
		int32 mcount = core.must_select_cards.size();
		for(int32 i = mcount; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			pgroup->container.insert(pcard);
		}
		pgroup->container.insert(core.must_select_cards.begin(), core.must_select_cards.end());
		core.must_select_cards.clear();
		pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
		pduel->restore_assumes();
		core.limit_tuner = 0;
		return TRUE;
	}
	case 6: {
		lua_pop(pduel->lua->current_state, 2);
		group* pgroup = pduel->new_group();
		pgroup->container.insert(core.limit_tuner);
		pgroup->container.insert(smat);
		pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
		pduel->restore_assumes();
		core.limit_tuner = 0;
		return TRUE;
	}
	case 7: {
		int32 lv = pcard->get_level();
		if(smat) {
			min--;
			max--;
		}
		if(returns.ivalue[0]) {
			effect* peffect = 0;
			for(auto cit = core.select_cards.begin(); cit != core.select_cards.end(); ++cit) {
				peffect = (*cit)->is_affected_by_effect(EFFECT_SCRAP_CHIMERA);
				if(peffect)
					break;
			}
			card_vector nsyn_filtered;
			for(auto cit = core.select_cards.begin(); cit != core.select_cards.end(); ++cit) {
				if(!peffect->get_value(*cit))
					nsyn_filtered.push_back(*cit);
			}
			core.select_cards.swap(nsyn_filtered);
		} else {
			card_vector nsyn_removed;
			for(auto cit = core.select_cards.begin(); cit != core.select_cards.end(); ++cit) {
				if(!(*cit)->is_affected_by_effect(EFFECT_SCRAP_CHIMERA))
					nsyn_removed.push_back(*cit);
			}
			core.select_cards.swap(nsyn_removed);
		}
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(512);
		add_process(PROCESSOR_SELECT_SUM, 0, 0, 0, lv, playerid + (min << 16) + (max << 24));
		core.units.begin()->step = 4;
		return FALSE;
	}
	}
	return TRUE;
}
int32 field::select_xyz_material(int16 step, uint8 playerid, uint32 lv, card* scard, int32 min, int32 max) {
	switch(step) {
	case 0: {
		int maxv = 0;
		if(core.xmaterial_lst.size())
			maxv = core.xmaterial_lst.begin()->first;
		if(min >= maxv) {
			core.select_cards.clear();
			for(auto iter = core.xmaterial_lst.begin(); iter != core.xmaterial_lst.end(); ++iter)
				core.select_cards.push_back(iter->second);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_SELECTMSG);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(513);
			add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid + ((uint32)core.summon_cancelable << 16), min + (max << 16));
		} else
			core.units.begin()->step = 1;
		return FALSE;
	}
	case 1: {
		if(returns.ivalue[0] == -1) {
			pduel->lua->add_param((void*)0, PARAM_TYPE_GROUP);
			return TRUE;
		}
		group* pgroup = pduel->new_group();
		for(int32 i = 0; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			pgroup->container.insert(pcard);
		}
		pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
		return TRUE;
	}
	case 2: {
		core.operated_set.clear();
		core.select_cards.clear();
		for(auto iter = core.xmaterial_lst.begin(); iter != core.xmaterial_lst.end(); ++iter)
			core.select_cards.push_back(iter->second);
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(513);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid + ((uint32)core.summon_cancelable << 16), min + (min << 16));
		return FALSE;
	}
	case 3: {
		if(returns.ivalue[0] == -1) {
			pduel->lua->add_param((void*)0, PARAM_TYPE_GROUP);
			return TRUE;
		}
		int32 pv = 0;
		for(int32 i = 0; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			core.operated_set.insert(pcard);
			for(auto iter = core.xmaterial_lst.begin(); iter != core.xmaterial_lst.end(); ++iter) {
				if(iter->second == pcard) {
					if(pv < iter->first)
						pv = iter->first;
					core.xmaterial_lst.erase(iter);
					break;
				}
			}
		}
		max -= returns.bvalue[0];
		if(max == 0 || core.xmaterial_lst.size() == 0) {
			group* pgroup = pduel->new_group(core.operated_set);
			pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
			return TRUE;
		}
		min = 0;
		if((int32)core.operated_set.size() < pv)
			min = pv - core.operated_set.size();
		core.units.begin()->arg2 = min + (max << 16);
		if(min == 0) {
			add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, playerid, 93);
			return FALSE;
		}
		returns.ivalue[0] = 1;
		return FALSE;
	}
	case 4: {
		if(!returns.ivalue[0]) {
			group* pgroup = pduel->new_group(core.operated_set);
			pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
			return TRUE;
		}
		core.select_cards.clear();
		for(auto iter = core.xmaterial_lst.begin(); iter != core.xmaterial_lst.end(); ++iter)
			core.select_cards.push_back(iter->second);
		int maxv = core.xmaterial_lst.begin()->first;
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(513);
		if(min == 0)
			min = 1;
		if(min + (int32)core.operated_set.size() >= maxv)
			add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid, min + (max << 16));
		else {
			add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, playerid, min + (min << 16));
			core.units.begin()->step = 2;
		}
		return FALSE;
	}
	case 5: {
		group* pgroup = pduel->new_group(core.operated_set);
		for(int32 i = 0; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			pgroup->container.insert(pcard);
		}
		pduel->lua->add_param(pgroup, PARAM_TYPE_GROUP);
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::select_release_cards(int16 step, uint8 playerid, uint8 check_field, uint8 cancelable, int32 min, int32 max) {
	switch(step) {
	case 0: {
		if(core.release_cards_ex.size() == 0 || (check_field && get_useable_count(playerid, LOCATION_MZONE, playerid, LOCATION_REASON_TOFIELD) <= 0)) {
			core.select_cards.clear();
			for(auto cit = core.release_cards.begin(); cit != core.release_cards.end(); ++cit)
				core.select_cards.push_back(*cit);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_SELECTMSG);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(500);
			add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (max << 16) + min);
			return TRUE;
		}
		if(core.release_cards_ex.size() >= (uint32)min) {
			core.select_cards.clear();
			for(auto cit = core.release_cards_ex.begin(); cit != core.release_cards_ex.end(); ++cit)
				core.select_cards.push_back(*cit);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_SELECTMSG);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(500);
			add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (max << 16) + min);
			return TRUE;
		}
		core.operated_set.clear();
		core.select_cards.clear();
		for(auto cit = core.release_cards_ex.begin(); cit != core.release_cards_ex.end(); ++cit)
			core.select_cards.push_back(*cit);
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(500);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (core.release_cards_ex.size() << 16) + core.release_cards_ex.size());
		min -= core.release_cards_ex.size();
		max -= core.release_cards_ex.size();
		core.units.begin()->arg2 = (max << 16) + min;
		return FALSE;
	}
	case 1: {
		for(int32 i = 0; i < returns.bvalue[0]; ++i)
			core.operated_set.insert(core.select_cards[returns.bvalue[i + 1]]);
		core.select_cards.clear();
		for(auto cit = core.release_cards.begin(); cit != core.release_cards.end(); ++cit)
			core.select_cards.push_back(*cit);
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(500);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (max << 16) + min);
		return FALSE;
	}
	case 2: {
		for(int32 i = 0; i < returns.bvalue[0]; ++i)
			core.operated_set.insert(core.select_cards[returns.bvalue[i + 1]]);
		core.select_cards.clear();
		returns.bvalue[0] = core.operated_set.size();
		int32 i = 0;
		for(auto cit = core.operated_set.begin(); cit != core.operated_set.end(); ++cit, ++i) {
			core.select_cards.push_back(*cit);
			returns.bvalue[i + 1] = i;
		}
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::select_tribute_cards(int16 step, uint8 playerid, uint8 cancelable, int32 min, int32 max) {
	switch(step) {
	case 0: {
		if(core.release_cards_ex.size() + core.release_cards_ex_sum.size() == 0
		        || (get_useable_count(playerid, LOCATION_MZONE, playerid, LOCATION_REASON_TOFIELD) <= 0 && min < 2)) {
			core.select_cards.clear();
			for(auto cit = core.release_cards.begin(); cit != core.release_cards.end(); ++cit)
				core.select_cards.push_back(*cit);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_SELECTMSG);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(500);
			add_process(PROCESSOR_SELECT_TRIBUTE_P, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (max << 16) + min);
			return TRUE;
		}
		if(core.release_cards_ex.size() >= (uint32)max) {
			core.select_cards.clear();
			for(auto cit = core.release_cards_ex.begin(); cit != core.release_cards_ex.end(); ++cit)
				core.select_cards.push_back(*cit);
			pduel->write_buffer8(MSG_HINT);
			pduel->write_buffer8(HINT_SELECTMSG);
			pduel->write_buffer8(playerid);
			pduel->write_buffer32(500);
			add_process(PROCESSOR_SELECT_TRIBUTE_P, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (max << 16) + min);
			return TRUE;
		}
		core.operated_set.clear();
		core.select_cards.clear();
		int32 rmax = 0;
		for(auto cit = core.release_cards.begin(); cit != core.release_cards.end(); ++cit)
			rmax += (*cit)->operation_param;
		for(auto cit = core.release_cards_ex.begin(); cit != core.release_cards_ex.end(); ++cit)
			rmax += (*cit)->operation_param;
		core.temp_var[0] = 0;
		if(rmax < min) {
			returns.ivalue[0] = TRUE;
			if(rmax == 0 && min == 2)
				core.temp_var[0] = 1;
		} else if(!core.release_cards_ex_sum.empty())
			add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, playerid, 92);
		else
			core.units.begin()->step = 2;
		return FALSE;
	}
	case 1: {
		if(!returns.ivalue[0]) {
			core.units.begin()->step = 2;
			return FALSE;
		}
		if(core.temp_var[0] == 0)
			for(auto cit = core.release_cards_ex_sum.begin(); cit != core.release_cards_ex_sum.end(); ++cit)
				core.select_cards.push_back(*cit);
		else
			for(auto cit = core.release_cards_ex_sum.begin(); cit != core.release_cards_ex_sum.end(); ++cit)
				if((*cit)->operation_param == 2)
					core.select_cards.push_back(*cit);
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(500);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, ((uint32)cancelable << 16) + playerid, 0x10001);
		return FALSE;
	}
	case 2: {
		if(returns.ivalue[0] == -1)
			return TRUE;
		for(int32 i = 0; i < returns.bvalue[0]; ++i) {
			card* pcard = core.select_cards[returns.bvalue[i + 1]];
			core.operated_set.insert(pcard);
			core.units.begin()->peffect = pcard->is_affected_by_effect(EFFECT_EXTRA_RELEASE_SUM);
		}
		return FALSE;
	}
	case 3: {
		for(auto cit = core.release_cards_ex.begin(); cit != core.release_cards_ex.end(); ++cit)
			core.select_cards.push_back(*cit);
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(500);
		add_process(PROCESSOR_SELECT_CARD, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (core.release_cards_ex.size() << 16) + core.release_cards_ex.size());
		return FALSE;
	}
	case 4: {
		if(returns.ivalue[0] == -1)
			return TRUE;
		for(int32 i = 0; i < returns.bvalue[0]; ++i)
			core.operated_set.insert(core.select_cards[returns.bvalue[i + 1]]);
		uint32 rmin = core.operated_set.size();
		uint32 rmax = 0;
		for(auto cit = core.operated_set.begin(); cit != core.operated_set.end(); ++cit)
			rmax += (*cit)->operation_param;
		min -= rmax;
		max -= rmin;
		core.units.begin()->arg2 = (max << 16) + min;
		if(min <= 0) {
			if(max > 0 && !core.release_cards.empty())
				add_process(PROCESSOR_SELECT_YESNO, 0, 0, 0, playerid, 210);
			else
				core.units.begin()->step = 6;
		} else
			returns.ivalue[0] = TRUE;
		return FALSE;
	}
	case 5: {
		if(!returns.ivalue[0]) {
			core.units.begin()->step = 6;
			return FALSE;
		}
		core.select_cards.clear();
		for(auto cit = core.release_cards.begin(); cit != core.release_cards.end(); ++cit)
			core.select_cards.push_back(*cit);
		pduel->write_buffer8(MSG_HINT);
		pduel->write_buffer8(HINT_SELECTMSG);
		pduel->write_buffer8(playerid);
		pduel->write_buffer32(500);
		add_process(PROCESSOR_SELECT_TRIBUTE_P, 0, 0, 0, ((uint32)cancelable << 16) + playerid, (max << 16) + min);
		return FALSE;
	}
	case 6: {
		if(returns.ivalue[0] == -1)
			return TRUE;
		for(int32 i = 0; i < returns.bvalue[0]; ++i)
			core.operated_set.insert(core.select_cards[returns.bvalue[i + 1]]);
		return FALSE;
	}
	case 7: {
		core.select_cards.clear();
		returns.bvalue[0] = core.operated_set.size();
		int32 i = 0;
		for(auto cit = core.operated_set.begin(); cit != core.operated_set.end(); ++cit, ++i) {
			core.select_cards.push_back(*cit);
			returns.bvalue[i + 1] = i;
		}
		effect* peffect = core.units.begin()->peffect;
		if(peffect)
			peffect->dec_count();
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::toss_coin(uint16 step, effect * reason_effect, uint8 reason_player, uint8 playerid, uint8 count) {
	switch(step) {
	case 0: {
		effect_set eset;
		effect* peffect = 0;
		tevent e;
		e.event_cards = 0;
		e.reason_effect = core.reason_effect;
		e.reason_player = core.reason_player;
		e.event_player = playerid;
		e.event_value = count;
		for(uint8 i = 0; i < 5; ++i)
			core.coin_result[i] = 0;
		filter_field_effect(EFFECT_TOSS_COIN_REPLACE, &eset);
		for(int32 i = eset.size() - 1; i >= 0; --i) {
			if(eset[i]->is_activateable(eset[i]->get_handler_player(), e)) {
				peffect = eset[i];
				break;
			}
		}
		if(!peffect) {
			pduel->write_buffer8(MSG_TOSS_COIN);
			pduel->write_buffer8(playerid);
			pduel->write_buffer8(count);
			for(int32 i = 0; i < count; ++i) {
				core.coin_result[i] = pduel->get_next_integer(0, 1);
				pduel->write_buffer8(core.coin_result[i]);
			}
			raise_event((card*)0, EVENT_TOSS_COIN_NEGATE, reason_effect, 0, reason_player, playerid, count);
			process_instant_event();
			return FALSE;
		} else {
			core.sub_solving_event.push_back(e);
			add_process(PROCESSOR_SOLVE_CONTINUOUS, 0, peffect, 0, peffect->get_handler_player(), 0);
			return TRUE;
		}
	}
	case 1: {
		raise_event((card*)0, EVENT_TOSS_COIN, reason_effect, 0, reason_player, playerid, count);
		process_instant_event();
		return TRUE;
	}
	}
	return TRUE;
}
int32 field::toss_dice(uint16 step, effect * reason_effect, uint8 reason_player, uint8 playerid, uint8 count1, uint8 count2) {
	switch(step) {
	case 0: {
		effect_set eset;
		effect* peffect = 0;
		tevent e;
		e.event_cards = 0;
		e.reason_effect = core.reason_effect;
		e.reason_player = core.reason_player;
		e.event_player = playerid;
		e.event_value = count1 + (count2 << 16);
		for(int32 i = 0; i < 5; ++i)
			core.dice_result[i] = 0;
		filter_field_effect(EFFECT_TOSS_DICE_REPLACE, &eset);
		for(int32 i = eset.size() - 1; i >= 0; --i) {
			if(eset[i]->is_activateable(eset[i]->get_handler_player(), e)) {
				peffect = eset[i];
				break;
			}
		}
		if(!peffect) {
			pduel->write_buffer8(MSG_TOSS_DICE);
			pduel->write_buffer8(playerid);
			pduel->write_buffer8(count1);
			for(int32 i = 0; i < count1; ++i) {
				core.dice_result[i] = pduel->get_next_integer(1, 6);
				pduel->write_buffer8(core.dice_result[i]);
			}
			if(count2 > 0) {
				pduel->write_buffer8(MSG_TOSS_DICE);
				pduel->write_buffer8(1 - playerid);
				pduel->write_buffer8(count2);
				for(int32 i = 0; i < count2; ++i) {
					core.dice_result[count1 + i] = pduel->get_next_integer(1, 6);
					pduel->write_buffer8(core.dice_result[count1 + i]);
				}
			}
			raise_event((card*)0, EVENT_TOSS_DICE_NEGATE, reason_effect, 0, reason_player, playerid, count1 + (count2 << 16));
			process_instant_event();
			return FALSE;
		} else {
			core.sub_solving_event.push_back(e);
			add_process(PROCESSOR_SOLVE_CONTINUOUS, 0, peffect, 0, peffect->get_handler_player(), 0);
			return TRUE;
		}
	}
	case 1: {
		raise_event((card*)0, EVENT_TOSS_DICE, reason_effect, 0, reason_player, playerid, count1 + (count2 << 16));
		process_instant_event();
		return TRUE;
	}
	}
	return TRUE;
}
