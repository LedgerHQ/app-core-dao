#include <stdbool.h>

#include "../bitcoin_app_base/src/boilerplate/dispatcher.h"
#include "../bitcoin_app_base/src/common/bitvector.h"
#include "../bitcoin_app_base/src/common/psbt.h"
#include "../bitcoin_app_base/src/handler/lib/get_merkleized_map.h"
#include "../bitcoin_app_base/src/handler/lib/get_merkleized_map_value.h"
#include "../bitcoin_app_base/src/handler/sign_psbt.h"
#include "../bitcoin_app_base/src/handler/sign_psbt/txhashes.h"
#include "../bitcoin_app_base/src/crypto.h"

#include "display.h"
#include "debug.h"
#include "core.h"

#define FLAG_OP_RETURN_FOUND 0x01
#define FLAG_LOCKING_OUTPUT_FOUND 0x01 << 1
#define FLAG_CHANGE_OUTPUT_FOUND 0x01 << 2

#define SCRIPT_PUBKEY_BUFFER_LEN 83 // Max length for OP_RETURN scriptPubKey
# define P2TR_SCRIPTPUBKEY_LEN 34

static core_dao_tx_info_t core_tx_info;

// No custom APDU
bool custom_apdu_handler(dispatcher_context_t *dc, const command_t *cmd) {
    UNUSED(dc), UNUSED(cmd);
    return false;
}

static bool get_output_amount(
    dispatcher_context_t *dc,
    merkleized_map_commitment_t *map,
    uint64_t *amount
) {
    uint8_t raw_amount[8];
    if (8 != call_get_merkleized_map_value(dc,
                                           map,
                                           (uint8_t[]){PSBT_OUT_AMOUNT},
                                           sizeof((uint8_t[]){PSBT_OUT_AMOUNT}),
                                           raw_amount,
                                           sizeof(raw_amount))) {
        SEND_SW(dc, SW_INCORRECT_DATA);
        return false;
    }
    *amount = read_u64_le(raw_amount, 0);
    return true;
}

static uint64_t read_amount(const uint8_t *buffer) {
    uint64_t amount = 0;
    amount |= (uint64_t)buffer[0] << 0;
    amount |= (uint64_t)buffer[1] << 8;
    amount |= (uint64_t)buffer[2] << 16;
    amount |= (uint64_t)buffer[3] << 24;
    amount |= (uint64_t)buffer[4] << 32;
    amount |= (uint64_t)buffer[5] << 40;
    amount |= (uint64_t)buffer[6] << 48;
    amount |= (uint64_t)buffer[7] << 56;
    return amount;
}

static bool get_utxo_witness(
    dispatcher_context_t *dc,
    merkleized_map_commitment_t *map,
    uint64_t *amount,
    uint8_t script_pubkey[static SCRIPT_HASH_LEN]
) {
    uint8_t utxo[43]; // 8 bytes amount; 1 byte length; 34 bytes P2TR Script
    if (sizeof(utxo) != call_get_merkleized_map_value(dc,
                                           map,
                                           (uint8_t[]){PSBT_IN_WITNESS_UTXO},
                                           sizeof((uint8_t[]){PSBT_IN_WITNESS_UTXO}),
                                           utxo,
                                           sizeof(utxo))) {
        PRINT("Unable to get witness UTXO or invalid witness UTXO\n");
        return false;
    }
    *amount = read_amount(utxo);
    if (utxo[8 + 0] != 34 || utxo[8 + 1] != OP_0 || utxo[8 + 2] != OP_PUSHBYTES_32) {
        PRINT("Unexpected scriptPubKey length in witness UTXO: %d\n", utxo[0]);
        return false;
    }
    memcpy(script_pubkey, utxo + 8 + 3, SCRIPT_HASH_LEN);
    return true;
}

static bool get_script_pubkey(
    dispatcher_context_t *dc,
    merkleized_map_commitment_t *map,
    uint8_t *script_pubkey,
    size_t script_pubkey_len,
    int *result_len
) {
    *result_len = call_get_merkleized_map_value(dc,
                                            map,
                                            (uint8_t[]){PSBT_OUT_SCRIPT},
                                            1,
                                            script_pubkey,
                                            script_pubkey_len);
    if (*result_len < 0) {
        SEND_SW(dc, SW_INCORRECT_DATA);
        return false;
    }
    return true;
}

static bool get_input_redeem_script(
    dispatcher_context_t *dc,
    merkleized_map_commitment_t *map,
    uint8_t *redeem_script,
    size_t redeem_script_len
) {
    if (REDEEM_SCRIPT_LEN != call_get_merkleized_map_value(dc,
                                           map,
                                           (uint8_t[]){PSBT_IN_WITNESS_SCRIPT},
                                           sizeof((uint8_t[]){PSBT_IN_WITNESS_SCRIPT}),
                                           redeem_script,
                                           redeem_script_len)) {
        SEND_SW(dc, SW_INCORRECT_DATA);
        return false;
    }
    return true;
}

static tx_type_t validate_lock_transaction(
    dispatcher_context_t *dc,
    sign_psbt_state_t *st,
    const uint8_t internal_outputs[64],
    core_dao_tx_info_t *info
) {
    int find = 0;
    merkleized_map_commitment_t external_output_map;
    uint8_t script_pubkey[SCRIPT_PUBKEY_BUFFER_LEN];
    int script_pubkey_len;
    uint8_t redeem_script[REDEEM_SCRIPT_LEN];
    size_t redeem_script_len = REDEEM_SCRIPT_LEN;
    uint8_t lock_script_pubkey[LOCK_SCRIPT_LEN];

    // Iterate through all outputs
    for (unsigned int i = 0; i < st->n_outputs; i++) {
        PRINT("Checking output %d\n", i);
        if (call_get_merkleized_map(dc, st->outputs_root, st->n_outputs, i, &external_output_map) < 0) {
            PRINT("Failed to get input\n");
            return TYPE_TX_INVALID;
        }
        // Get output amount
        uint64_t amount;
        if (!get_output_amount(dc, &external_output_map, &amount)) {
            return TYPE_TX_INVALID;
        }
        // Get output scriptPubKey
        if (!get_script_pubkey(dc, &external_output_map, script_pubkey,
                               sizeof(script_pubkey), &script_pubkey_len)) {
            PRINT("Failed to get scriptPubKey for output %d\n", i);
            return TYPE_TX_INVALID;
        }
        if (script_pubkey[0] == OP_RETURN) {
            if (!parse_staking_information(script_pubkey + 3, 
                                           script_pubkey_len - 3,
                                           info,
                                           redeem_script) || amount != 0) {
                PRINT_HEX(script_pubkey, script_pubkey_len, "OP_RETURN scriptPubKey: ");
                PRINT("Invalid OP_RETURN output or amount is not at zero\n");
                SEND_SW(dc, SW_INCORRECT_DATA);
                return TYPE_TX_INVALID;
            }
            find |= FLAG_OP_RETURN_FOUND;
        } else if (bitvector_get(internal_outputs, i) == 1) {
            // If the output is internal, consider it to be the change
            find |= FLAG_CHANGE_OUTPUT_FOUND;
        } else {
            if (script_pubkey_len != LOCK_SCRIPT_LEN) {
                PRINT("Invalid scriptPubKey length for locking output (%d)\n", i);
                SEND_SW(dc, SW_INCORRECT_DATA);
                return TYPE_TX_INVALID;
            }
            memcpy(lock_script_pubkey, script_pubkey, LOCK_SCRIPT_LEN);
            find |= FLAG_LOCKING_OUTPUT_FOUND;
        }
    }
    
    // If a lock output and a valid op return was found the tx is a staking tx
    if (find & FLAG_OP_RETURN_FOUND && find & FLAG_LOCKING_OUTPUT_FOUND) {
        PRINT("Staking transaction\n");
        info->type |= TYPE_TX_LOCK;
    } else {
        PRINT("NOT A STAKING TX\n");
        return info->type;
    }

    if (info->type & TYPE_TX_LOCK && (st->n_outputs < 2 || st->n_outputs > 3)) {
        PRINT("Invalid number of outputs\n");
        SEND_SW(dc, SW_INCORRECT_DATA);
        return TYPE_TX_INVALID;
    }

    info->lock_amount = st->internal_inputs_total_amount - st->outputs.change_total_amount;
    
    PRINT_HEX(info->delegator, 20, "Delegator: ");
    PRINT_HEX(info->validator, 20, "Validator: ");
    PRINT("Amount: %llu\n", info->lock_amount);
    PRINT("Fee: %d\n", info->fee);
    PRINT_HEX(redeem_script, redeem_script_len, "Redeem script: ");
    PRINT_HEX(lock_script_pubkey, LOCK_SCRIPT_LEN, "Lock scriptPubKey: ");
    
    // Verify the redeem script contains the expected public key
    if (!validate_redeem_script(redeem_script)) {
        PRINT("Invalid redeem script in OP_RETURN output\n");
        SEND_SW(dc, SW_INCORRECT_DATA);
        return TYPE_TX_INVALID;
    }

    // Verify the lock output uses the right redeem script
    if (!validate_lock_script_pubkey(lock_script_pubkey, LOCK_SCRIPT_LEN, redeem_script)) {
        PRINT("Invalid scriptPubKey for the lock output\n");
        SEND_SW(dc, SW_INCORRECT_DATA);
        return TYPE_TX_INVALID;
    }

    return info->type;
}

static tx_type_t validate_unlock_transaction(
    dispatcher_context_t *dc,
    sign_psbt_state_t *st,
    const uint8_t internal_inputs[64],
    core_dao_tx_info_t *info
) {
    merkleized_map_commitment_t external_input_map;
    // Count the number of CoreDAO inputs
    info->n_core_dao_inputs = 0;
    info->unlock_amount = 0;
    for (unsigned int i = 0; i < st->n_inputs; i++) {
        PRINT("Checking input %d\n", i);
        if (bitvector_get(internal_inputs, i) == 0) {
            // Verify if the input is a CoreDAO input (fail otherwise)
            // Get commitment to the i-th input's map
            PRINT("Getting input %d\n", i);
            if (call_get_merkleized_map(dc, st->inputs_root, st->n_inputs, i, &external_input_map) < 0) {
                PRINT("Failed to get input &d\n", i);
                return TYPE_TX_INVALID;
            }
            // Get input amount and redeem script
            uint64_t amount;
            uint8_t redeem_script_hash[SCRIPT_HASH_LEN];
            uint8_t redeem_script[REDEEM_SCRIPT_LEN];

            if (!get_utxo_witness(dc, &external_input_map, &amount, redeem_script_hash)) {
                return TYPE_TX_INVALID;
            }

            if (!get_input_redeem_script(dc, &external_input_map, redeem_script, REDEEM_SCRIPT_LEN)) {
                return TYPE_TX_INVALID;
            }

            // Check if the redeem script is a valid CoreDAO redeem script
            if (!validate_redeem_script(redeem_script)) {
                PRINT("Invalid redeem script in input %d\n", i);
                return TYPE_TX_INVALID;
            }

            info->type |= TYPE_TX_UNLOCK;
            info->n_core_dao_inputs += 1;
            info->unlock_amount += amount;
            bitvector_set(info->core_inputs, i, 1); // Mark the input as a CoreDAO input
        } else {
            PRINT("Internal input %d\n", i);
        }
    }

    PRINT("Unlock amount: %llu\n", info->unlock_amount);

    return info->type;
}

static tx_type_t validate_transaction(
    dispatcher_context_t *dc,
    sign_psbt_state_t *st,
    const uint8_t internal_inputs[64],
    const uint8_t internal_outputs[64],
    core_dao_tx_info_t *info) {
    // This application implements the following rules:
    // - If a transaction contains a OP_RETURN output, It must be a valid CoreDAO output
    // - If a transaction contains a OP_RETURN output, It must have a locking output
    // - The PSBT can have at most 1 change output
    // - The PSBT can have any number of CoreDAO inputs
    // - The PSBT can have any number of internal inputs
    // - If at least one input is a CoreDAO input, outputs can only be internal or lock output

    tx_type_t tx_type = TYPE_TX_UNKNOWN;
    PRINT("Validating transaction\n");
    tx_type |= validate_unlock_transaction(dc, st, internal_inputs, info);
    if (!(tx_type & TYPE_TX_INVALID)) {
        tx_type |= validate_lock_transaction(dc, st, internal_outputs, info);
    }
    return tx_type;
}

// hooking into a weak function
bool validate_and_display_transaction(
    dispatcher_context_t *dc,
    sign_psbt_state_t *st,
    const uint8_t internal_inputs[64],
    const uint8_t internal_outputs[64]) {
    PRINT("Validating and displaying transaction\n");


    explicit_bzero(&core_tx_info, sizeof(core_tx_info));

    tx_type_t tx_type = validate_transaction(dc, st, internal_inputs, internal_outputs, &core_tx_info);

    if ((tx_type & TYPE_TX_INVALID) == TYPE_TX_INVALID) {
        PRINT("Send invalid status\n");
        SEND_SW(dc, SW_INCORRECT_DATA);
        return false;
    }

    // the amount spent from the wallet policy (or negative if the it received more funds than it spent)
    int64_t internal_value = st->internal_inputs_total_amount + 
                             core_tx_info.unlock_amount -
                             st->outputs.change_total_amount;

    uint64_t fee = st->inputs_total_amount - st->outputs.total_amount;

    if (!display_transaction(dc, internal_value, fee, &core_tx_info)) {
        return false;
    }

    return true;
}


bool sign_custom_inputs(
    dispatcher_context_t *dc,
    sign_psbt_state_t *st,
    tx_hashes_t *tx_hashes,
    const uint8_t internal_inputs[static BITVECTOR_REAL_SIZE(MAX_N_INPUTS_CAN_SIGN)]) {
    UNUSED(dc), UNUSED(st), UNUSED(tx_hashes), UNUSED(internal_inputs);
    merkleized_map_commitment_t input_map;
    uint8_t sighash[32];
    uint8_t redeem_script[REDEEM_SCRIPT_LEN];
    uint32_t path[] = CORE_DERIVATION_PATH;
    uint64_t amount;
    uint8_t script_pubkey[2 + SCRIPT_HASH_LEN];

    for (size_t i = 0; i < st->n_inputs; i++) {
        if (bitvector_get(core_tx_info.core_inputs, i) == 1) {
            PRINT("Signing input %d\n", i);
            // Get the commitment to the i-th input's map
            if (call_get_merkleized_map(dc, st->inputs_root, st->n_inputs, i, &input_map) < 0) {
                PRINT("Failed to get input &d\n", i);
                SEND_SW(dc, SW_INCORRECT_DATA);
                return false;
            }
            // Get the redeem script
            if (!get_utxo_witness(dc, &input_map, &amount, script_pubkey + 2)) {
                SEND_SW(dc, SW_INCORRECT_DATA);
                return false;
            }
            script_pubkey[0] = 0x00;
            script_pubkey[1] = 0x20;
            PRINT_HEX(script_pubkey, SCRIPT_HASH_LEN + 2, "Redeem script: ");
            if (!compute_sighash_segwitv0(
                dc,
                st,
                tx_hashes,
                &input_map,
                i,
                script_pubkey,
                SCRIPT_HASH_LEN + 2,
                SIGHASH_DEFAULT,
                sighash
            )) {
                PRINT("Failed to compute the sighash\n");
                return false;
            }
            if (!sign_sighash_ecdsa_and_yield(
                dc,
                st,
                i,
                path,
                CORE_DERIVATION_PATH_LEN,
                SIGHASH_DEFAULT,
                sighash)) {
                PRINT("Signing failed\n");
                return false;
            }
        }
    }
    return true;
}