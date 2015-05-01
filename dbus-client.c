/*
 * This uses the recommended GLib API for D-Bus: GDBus,
 * which has been distributed with GLib since version 2.26.
 *
 * For details of how to use GDBus, see:
 * https://developer.gnome.org/gio/stable/gdbus-convenience.html
 *
 * dbus-glib also exists but is deprecated.
 */
#include <stdbool.h>
#include <stdio.h>
#include <glib/gprintf.h>
#include <gio/gio.h>


void test_Ping(GDBusProxy *proxy)
{
	GVariant *result;
	GError *error = NULL;
	const gchar *str;

	g_printf("Calling Ping()...\n");
	result = g_dbus_proxy_call_sync(proxy,
					"Ping",
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	g_assert_no_error(error);
	g_variant_get(result, "(&s)", &str);
	g_printf("The server answered: '%s'\n", str);
	g_variant_unref(result);
}


void test_Echo(GDBusProxy *proxy)
{
	GVariant *result;
	GError *error = NULL;
	const gchar *str;

	g_printf("Calling Echo('1234')...\n");
	result = g_dbus_proxy_call_sync(proxy,
					"Echo",
					g_variant_new ("(s)", "1234"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	g_assert_no_error(error);
	g_variant_get(result, "(&s)", &str);
	g_printf("The server answered: '%s'\n", str);
	g_variant_unref(result);
}


void on_emit_signal_callback(GDBusConnection *conn,
			     const gchar *sender_name,
			     const gchar *object_path,
			     const gchar *interface_name,
			     const gchar *signal_name,
			     GVariant *parameters,
			     gpointer data)
{
	GMainLoop *loop = data;

	g_printf("signal handler: OnEmitSignal received.\n");
	g_main_loop_quit(loop);
}


/*
 * Make the server emit a signal and catch it. In order to detect that
 * the callback has been run (and thus the signal emitted by the
 * server), we use a glib event loop to:
 *
 *    a) allow the signal callback to be called;
 *
 *    b) make the callback to break the local glib loop so exiting the
 *       loop indicates that the signal has been received.
 */
void test_EmitSignal(GDBusProxy *proxy)
{
	GMainLoop *loop;
	GError *error = NULL;
	guint id; /* subscription id */
	GDBusConnection *conn;

	loop = g_main_loop_new(NULL, false);
	conn = g_dbus_proxy_get_connection(proxy);

	id = g_dbus_connection_signal_subscribe(conn,
						"org.example.TestServer",
						"org.example.TestInterface",
						"OnEmitSignal",
						"/org/example/TestObject",
						NULL, /* arg0 */
						G_DBUS_SIGNAL_FLAGS_NONE,
						on_emit_signal_callback,
						loop, /* user data */
						NULL);

	/*
	 * Make the server emit the signal. Normally no races can
	 * happen here since signal events are only processed once the
	 * loop is started so the callback can't be run before.
	 */
	g_printf("Calling method EmitSignal()...\n");
	g_dbus_proxy_call_sync(proxy,
			       "EmitSignal",
			       NULL,	/* no arguments */
			       G_DBUS_CALL_FLAGS_NONE,
			       -1,
			       NULL,
			       &error);
	g_assert_no_error(error);

	/*
	 * The only way to break the loop is to receive the signal and
	 * run the signal's callback.
	 */
	g_main_loop_run(loop);
	g_dbus_connection_signal_unsubscribe(conn, id);
	g_printf("The server emitted 'OnEmitSignal'\n");
}


void test_Quit(GDBusProxy *proxy)
{
	GVariant *result;
	GError *error = NULL;

	g_printf("Calling method Quit()...\n");
	result = g_dbus_proxy_call_sync(proxy,
					"Quit",
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	g_assert_no_error(error);
	g_variant_unref(result);
}


int main(void)
{
	GDBusProxy *proxy;
	GDBusConnection *conn;
	GError *error = NULL;
	const char *version;
	GVariant *variant;

	conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error(error);

	proxy = g_dbus_proxy_new_sync(conn,
				      G_DBUS_PROXY_FLAGS_NONE,
				      NULL,				/* GDBusInterfaceInfo */
				      "org.example.TestServer",		/* name */
				      "/org/example/TestObject",	/* object path */
				      "org.example.TestInterface",	/* interface */
				      NULL,				/* GCancellable */
				      &error);
	g_assert_no_error(error);

	/* read the version property of the interface */
	variant = g_dbus_proxy_get_cached_property(proxy, "Version");
	g_assert(variant != NULL);
	g_variant_get(variant, "s", &version);
	g_variant_unref(variant);
	printf("Testing server interface v%s\n", version);

	/* Test all server methods */
	test_Ping(proxy);
	test_Echo(proxy);
	test_EmitSignal(proxy);
	test_Quit(proxy);

	g_object_unref(proxy);
	g_object_unref(conn);
	return 0;
}
