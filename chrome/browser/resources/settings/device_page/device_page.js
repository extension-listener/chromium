// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
Polymer({
  is: 'settings-device-page',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * |hasMouse_|, |hasTouchpad_|, and |hasStylus_| start undefined so
     * observers don't trigger until they have been populated.
     * @private
     */
    hasMouse_: {
      type: Boolean,
      value: false
    },

    /** @private */
    hasTouchpad_: {
      type: Boolean,
      value: false
    },

    /** @private */
    hasStylus_: {
      type: Boolean,
      value: false
    },

    /**
     * Whether power status and settings should be fetched and displayed.
     * @private
     */
    enablePowerSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enablePowerSettings');
      },
      readOnly: true,
    },

    /** @private {string} ID of the selected power source, or ''. */
    selectedPowerSourceId_: String,

    /** @private {!settings.BatteryStatus|undefined} */
    batteryStatus_: Object,

    /** @private {boolean} Whether a low-power (USB) charger is being used. */
    lowPowerCharger_: Boolean,

    /**
     * List of available dual-role power sources, if enablePowerSettings_ is on.
     * @private {!Array<!settings.PowerSource>|undefined}
     */
    powerSources_: Array,

    /** @private */
    powerLabel_: {
      type: String,
      computed: 'computePowerLabel_(powerSources_, batteryStatus_.calculating)',
    },

    /** @private */
    showPowerDropdown_: {
      type: Boolean,
      computed: 'computeShowPowerDropdown_(powerSources_)',
      value: false,
    },

    /**
     * The name of the dedicated charging device being used, if present.
     * @private {string}
     */
    powerSourceName_: {
      type: String,
      computed: 'computePowerSourceName_(powerSources_, lowPowerCharger_)',
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasTouchpad_)',
  ],

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    this.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializePointers();

    this.addWebUIListener(
        'has-stylus-changed', this.set.bind(this, 'hasStylus_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializeStylus();

    if (this.enablePowerSettings_) {
      this.addWebUIListener(
          'battery-status-changed', this.set.bind(this, 'batteryStatus_'));
      this.addWebUIListener(
          'power-sources-changed', this.powerSourcesChanged_.bind(this));
      settings.DevicePageBrowserProxyImpl.getInstance().updatePowerStatus();
    }
  },

  /**
   * @return {string}
   * @private
   */
  getPointersTitle_: function() {
    if (this.hasMouse_ && this.hasTouchpad_)
      return this.i18n('mouseAndTouchpadTitle');
    if (this.hasMouse_)
      return this.i18n('mouseTitle');
    if (this.hasTouchpad_)
      return this.i18n('touchpadTitle');
    return '';
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   */
  isEqual_: function(lhs, rhs) {
    return lhs === rhs;
  },

  /**
   * @param {!Array<!settings.PowerSource>|undefined} powerSources
   * @param {boolean} calculating
   * @return {string} The primary label for the power row.
   * @private
   */
  computePowerLabel_: function(powerSources, calculating) {
    return this.i18n(calculating ? 'calculatingPower' :
        powerSources.length ? 'powerSourceLabel' : 'powerSourceBattery');
  },

  /**
   * @param {!Array<!settings.PowerSource>} powerSources
   * @return {boolean} True if at least one power source is attached and all of
   *     them are dual-role (no dedicated chargers).
   * @private
   */
  computeShowPowerDropdown_: function(powerSources) {
    return powerSources.length > 0 && powerSources.every(function(source) {
      return source.type == settings.PowerDeviceType.DUAL_ROLE_USB;
    });
  },

  /**
   * @param {!Array<!settings.PowerSource>} powerSources
   * @param {boolean} lowPowerCharger
   * @return {string} Description of the power source.
   * @private
   */
  computePowerSourceName_: function (powerSources, lowPowerCharger) {
    if (lowPowerCharger)
      return this.i18n('powerSourceLowPowerCharger');
    if (powerSources.length)
      return this.i18n('powerSourceAcAdapter');
    return '';
  },

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   * @private
   */
  onPointersTap_: function() {
    settings.navigateTo(settings.Route.POINTERS);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onKeyboardTap_: function() {
    settings.navigateTo(settings.Route.KEYBOARD);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onStylusTap_: function() {
    settings.navigateTo(settings.Route.STYLUS);
  },

  /**
   * Handler for tapping the Display settings menu item.
   * @private
   */
  onDisplayTap_: function() {
    settings.navigateTo(settings.Route.DISPLAY);
  },

  /**
   * Handler for tapping the Storage settings menu item.
   * @private
   */
  onStorageTap_: function() {
    settings.navigateTo(settings.Route.STORAGE);
  },

  onPowerSourceChange_: function() {
    settings.DevicePageBrowserProxyImpl.getInstance().setPowerSource(
        this.$$('#powerSource').value);
  },

  /** @protected */
  currentRouteChanged: function() {
    this.checkPointerSubpage_();
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_: function(hasMouse, hasTouchpad) {
    this.$.pointersRow.hidden = !hasMouse && !hasTouchpad;
    this.checkPointerSubpage_();
  },

  /**
   * @param {!Array<settings.PowerSource>} sources External power sources.
   * @param {string} selectedId The ID of the currently used power source.
   * @param {boolean} lowPowerCharger Whether the currently used power source
   *     is a low-powered USB charger.
   * @private
   */
  powerSourcesChanged_: function(sources, selectedId, lowPowerCharger) {
    this.powerSources_ = sources;
    this.selectedPowerSourceId_ = selectedId;
    this.lowPowerCharger_ = lowPowerCharger;
  },

  /**
   * Leaves the pointer subpage if all pointing devices are detached.
   * @private
   */
  checkPointerSubpage_: function() {
    // Check that the properties have explicitly been set to false.
    if (this.hasMouse_ === false && this.hasTouchpad_ === false &&
        settings.getCurrentRoute() == settings.Route.POINTERS) {
      settings.navigateTo(settings.Route.DEVICE);
    }
  },
});
