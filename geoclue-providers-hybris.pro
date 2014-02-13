TEMPLATE = subdirs

hybris_dbus.file = hybris_dbus.pro
control_dbus.file = control_dbus.pro
provider.file = provider.pro
provider.depends += hybris_dbus control_dbus

SUBDIRS = provider hybris_dbus control_dbus

OTHER_FILES = \
    rpm/geoclue-providers-hybris.spec
