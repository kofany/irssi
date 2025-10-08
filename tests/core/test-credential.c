/*
 test-credential.c : Unit tests for credential management

    Copyright (C) 2024 Irssi Project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <glib.h>
#include <string.h>
#include <stdio.h>

/* Mock includes - w prawdziwym teście trzeba by dodać odpowiednie ścieżki */
#include "../../src/core/credential.h"

/* === Testy podstawowych funkcji === */

static void test_credential_context_conversion(void)
{
	CredentialContext context;
	const char *str;
	
	/* Test konwersji context -> string */
	str = credential_context_to_string(CREDENTIAL_CONTEXT_SERVER_PASSWORD);
	g_assert_cmpstr(str, ==, "server_password");
	
	str = credential_context_to_string(CREDENTIAL_CONTEXT_SASL_PASSWORD);
	g_assert_cmpstr(str, ==, "sasl_password");
	
	/* Test konwersji string -> context */
	context = credential_string_to_context("server_password");
	g_assert_cmpint(context, ==, CREDENTIAL_CONTEXT_SERVER_PASSWORD);
	
	context = credential_string_to_context("sasl_password");
	g_assert_cmpint(context, ==, CREDENTIAL_CONTEXT_SASL_PASSWORD);
	
	/* Test nieprawidłowego stringa */
	context = credential_string_to_context("invalid_context");
	g_assert_cmpint(context, ==, CREDENTIAL_CONTEXT_SERVER_PASSWORD);
}

static void test_credential_storage_mode_conversion(void)
{
	const char *str;
	
	str = credential_storage_mode_to_string(CREDENTIAL_STORAGE_CONFIG);
	g_assert_cmpstr(str, ==, "config");
	
	str = credential_storage_mode_to_string(CREDENTIAL_STORAGE_EXTERNAL);
	g_assert_cmpstr(str, ==, "external");
	
	str = credential_storage_mode_to_string(CREDENTIAL_STORAGE_EXTERNAL_ENCRYPTED);
	g_assert_cmpstr(str, ==, "external_encrypted");
}

static void test_credential_master_password(void)
{
	/* Test ustawiania hasła głównego */
	g_assert_false(credential_has_master_password());
	
	g_assert_true(credential_set_master_password("test_password"));
	g_assert_true(credential_has_master_password());
	
	/* Test czyszczenia hasła */
	credential_clear_master_password();
	g_assert_false(credential_has_master_password());
}

static void test_credential_sensitive_field_detection(void)
{
	/* Test wykrywania pól wrażliwych */
	g_assert_true(credential_is_sensitive_field("password", NULL));
	g_assert_true(credential_is_sensitive_field("sasl_password", NULL));
	g_assert_true(credential_is_sensitive_field("sasl_username", NULL));
	g_assert_true(credential_is_sensitive_field("proxy_password", NULL));
	
	g_assert_false(credential_is_sensitive_field("nick", NULL));
	g_assert_false(credential_is_sensitive_field("real_name", NULL));
	
	/* Test autosendcmd */
	g_assert_true(credential_is_sensitive_field("autosendcmd", "NickServ identify mypass"));
	g_assert_true(credential_is_sensitive_field("autosendcmd", "PRIVMSG NickServ :identify mypass"));
	g_assert_false(credential_is_sensitive_field("autosendcmd", "JOIN #channel"));
}

static void test_credential_autosendcmd_detection(void)
{
	/* Test wykrywania wrażliwych komend autosendcmd */
	g_assert_true(credential_is_autosendcmd_sensitive("NickServ identify mypassword"));
	g_assert_true(credential_is_autosendcmd_sensitive("PRIVMSG NickServ :identify mypass"));
	g_assert_true(credential_is_autosendcmd_sensitive("MSG NickServ identify test"));
	g_assert_true(credential_is_autosendcmd_sensitive("Q@CServe.quakenet.org AUTH user pass"));
	
	g_assert_false(credential_is_autosendcmd_sensitive("JOIN #channel"));
	g_assert_false(credential_is_autosendcmd_sensitive("PRIVMSG #channel :hello"));
	g_assert_false(credential_is_autosendcmd_sensitive("MODE +i"));
}

static void test_credential_basic_operations(void)
{
	GSList *list;
	char *value;
	
	/* Test ustawiania i pobierania credentials */
	g_assert_true(credential_set("testnet", CREDENTIAL_CONTEXT_SERVER_PASSWORD, "testpass"));
	
	value = credential_get("testnet", CREDENTIAL_CONTEXT_SERVER_PASSWORD);
	g_assert_nonnull(value);
	g_assert_cmpstr(value, ==, "testpass");
	g_free(value);
	
	/* Test listy credentials */
	list = credential_list();
	g_assert_nonnull(list);
	g_assert_cmpint(g_slist_length(list), ==, 1);
	g_slist_free(list);
	
	/* Test usuwania credentials */
	g_assert_true(credential_remove("testnet", CREDENTIAL_CONTEXT_SERVER_PASSWORD));
	
	value = credential_get("testnet", CREDENTIAL_CONTEXT_SERVER_PASSWORD);
	g_assert_null(value);
	
	/* Test usuwania nieistniejącego */
	g_assert_false(credential_remove("nonexistent", CREDENTIAL_CONTEXT_SERVER_PASSWORD));
}

/* === Funkcja główna testów === */

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	
	/* Inicjalizacja systemu credential dla testów */
	credential_init();
	
	/* Rejestracja testów */
	g_test_add_func("/credential/context_conversion", test_credential_context_conversion);
	g_test_add_func("/credential/storage_mode_conversion", test_credential_storage_mode_conversion);
	g_test_add_func("/credential/master_password", test_credential_master_password);
	g_test_add_func("/credential/sensitive_field_detection", test_credential_sensitive_field_detection);
	g_test_add_func("/credential/autosendcmd_detection", test_credential_autosendcmd_detection);
	g_test_add_func("/credential/basic_operations", test_credential_basic_operations);
	
	/* Uruchomienie testów */
	int result = g_test_run();
	
	/* Czyszczenie */
	credential_deinit();
	
	return result;
}