/* 
 * Version: MPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Event Handler Software Library / Soft-Switch Application
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Cui Gang <cuigang0120@aliyun.com>
 *
 *
 *
 */

#include <switch.h>
#include <zmq.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_event_zeromq_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_zeromq_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_zeromq_runtime);
SWITCH_MODULE_DEFINITION(mod_event_zeromq, mod_event_zeromq_load, mod_event_zeromq_shutdown, mod_event_zeromq_runtime);

static struct {
	switch_event_node_t *node;
	int debug;
} globals;

static struct {
	char *endpoint;
} prefs;

// ZeroMQ
static void* run_flag;		// zmq_atomic_counter 
static void* ctx_pub;		// zmq_ctx
static void* socket_publisher;

static void free_message_data(void *data, void *hint) 
{
		switch_safe_free(data);
}

static void event_handler(switch_event_t *event)
{
	// Serialize the event into a JSON string
	char* pjson;
	switch_event_serialize_json(event, &pjson);

	// Use the JSON string as the message body
	zmq_msg_t msg;
	if(zmq_msg_init_data(&msg, pjson, strlen(pjson), free_message_data, NULL)==0){
		// Send the message
		zmq_msg_send(&msg, socket_publisher, 0);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ZeroMQ Event Handled.\n");
	}else{
		free_message_data(pjson, NULL);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "zmq_msg_init_data failed. Event message lost.\n");
	}
}

static void free_prefs()
{
	switch_safe_free(prefs.endpoint);
}

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_endpoint, prefs.endpoint);

static switch_status_t config(void)
{
	char *cf = "event_zeromq.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&prefs, 0, sizeof(prefs));

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "endpoint")) {
					set_pref_endpoint(val);
				}
			}
		}
		switch_xml_free(xml);
	}

	if (zstr(prefs.endpoint)) {
		set_pref_endpoint("tcp://127.0.0.1:5556");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_event_zeromq_load)
{
	int major, minor, patch;
	zmq_version (&major, &minor, &patch);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ZeroMQ version is %d.%d.%d\n", major, minor, patch);

	// Load config XML
	config();

	// Set up the ØMQ 
	ctx_pub = zmq_ctx_new();
	socket_publisher = zmq_socket(ctx_pub, ZMQ_PUB);
	
	if( zmq_bind(socket_publisher, prefs.endpoint) != 0 ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ZeroMQ socket bind failed. endpoint is %s\n", prefs.endpoint);
		zmq_close(socket_publisher);
		zmq_ctx_term(ctx_pub);
		return SWITCH_STATUS_GENERR;
	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ZeroMQ bind endpoint is %s\n", prefs.endpoint);
	}

	run_flag = zmq_atomic_counter_new();
	if(run_flag==NULL){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading ZeroMQ module, zmq_atomic_counter_new return NULL\n");
		return SWITCH_STATUS_GENERR;
	}
	zmq_atomic_counter_set(run_flag, 0);
	ctx_pub = zmq_ctx_new();

	memset(&globals, 0, sizeof(globals));
	// Subscribe to all switch events of any subclass
	// Store a pointer to ourself in the user data
	if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module loaded\n");
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_zeromq_runtime) 
{

	while(zmq_atomic_counter_value(run_flag)==0) {
		switch_yield(100000);
	}
	zmq_atomic_counter_set(run_flag, 2);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_event_zeromq runloop end!\n");
	// Tell the switch to stop calling this runtime loop
	return SWITCH_STATUS_TERM;
}

SWITCH_MOD_DECLARE(switch_status_t) mod_event_zeromq_shutdown(void)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutdown requested, sending term message to runloop\n");
	zmq_atomic_counter_set(run_flag, 1);
	while(zmq_atomic_counter_value(run_flag)==1) {
		switch_yield(10000);
	}
	zmq_atomic_counter_destroy(&run_flag);

	zmq_close(socket_publisher);
	zmq_ctx_term(ctx_pub);

	switch_event_unbind(&globals.node);
	
	free_prefs();

	return SWITCH_STATUS_SUCCESS;
}

