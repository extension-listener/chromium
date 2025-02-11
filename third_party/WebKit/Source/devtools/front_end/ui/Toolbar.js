/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
UI.Toolbar = class {
  /**
   * @param {string} className
   * @param {!Element=} parentElement
   */
  constructor(className, parentElement) {
    /** @type {!Array.<!UI.ToolbarItem>} */
    this._items = [];
    this._reverse = false;
    this.element = parentElement ? parentElement.createChild('div') : createElement('div');
    this.element.className = className;
    this.element.classList.add('toolbar');
    this._enabled = true;
    this._shadowRoot = UI.createShadowRootWithCoreStyles(this.element, 'ui/toolbar.css');
    this._contentElement = this._shadowRoot.createChild('div', 'toolbar-shadow');
    this._insertionPoint = this._contentElement.createChild('content');
  }

  /**
   * @param {!UI.Action} action
   * @param {!Array<!UI.ToolbarButton>=} toggledOptions
   * @param {!Array<!UI.ToolbarButton>=} untoggledOptions
   * @return {!UI.ToolbarToggle}
   */
  static createActionButton(action, toggledOptions, untoggledOptions) {
    var button = new UI.ToolbarToggle(action.title(), action.icon(), action.toggledIcon());
    button.setToggleWithRedColor(action.toggleWithRedColor());
    button.addEventListener(UI.ToolbarButton.Events.Click, action.execute, action);
    action.addEventListener(UI.Action.Events.Enabled, enabledChanged);
    action.addEventListener(UI.Action.Events.Toggled, toggled);
    /** @type {?UI.LongClickController} */
    var longClickController = null;
    /** @type {?Array<!UI.ToolbarButton>} */
    var longClickButtons = null;
    /** @type {?Element} */
    var longClickGlyph = null;
    toggled();
    return button;

    /**
     * @param {!Common.Event} event
     */
    function enabledChanged(event) {
      button.setEnabled(/** @type {boolean} */ (event.data));
    }

    function toggled() {
      button.setToggled(action.toggled());
      if (action.title())
        UI.Tooltip.install(button.element, action.title(), action.id());
      updateOptions();
    }

    function updateOptions() {
      var buttons = action.toggled() ? (toggledOptions || null) : (untoggledOptions || null);

      if (buttons && buttons.length) {
        if (!longClickController) {
          longClickController = new UI.LongClickController(button.element, showOptions);
          longClickGlyph = UI.Icon.create('largeicon-longclick-triangle', 'long-click-glyph');
          button.element.appendChild(longClickGlyph);
          longClickButtons = buttons;
        }
      } else {
        if (longClickController) {
          longClickController.dispose();
          longClickController = null;
          longClickGlyph.remove();
          longClickGlyph = null;
          longClickButtons = null;
        }
      }
    }

    function showOptions() {
      var buttons = longClickButtons.slice();
      var mainButtonClone = new UI.ToolbarToggle(action.title(), action.icon(), action.toggledIcon());
      mainButtonClone.addEventListener(UI.ToolbarButton.Events.Click, clicked);

      /**
       * @param {!Common.Event} event
       */
      function clicked(event) {
        button._clicked(/** @type {!Event} */ (event.data));
      }

      mainButtonClone.setToggled(action.toggled());
      buttons.push(mainButtonClone);

      var document = button.element.ownerDocument;
      document.documentElement.addEventListener('mouseup', mouseUp, false);

      var optionsGlassPane = new UI.GlassPane(document, false /* dimmed */, true /* blockPointerEvents */, event => {});
      optionsGlassPane.show();
      var optionsBar = new UI.Toolbar('fill', optionsGlassPane.contentElement);
      optionsBar._contentElement.classList.add('floating');
      const buttonHeight = 26;

      var hostButtonPosition = button.element.totalOffset();

      var topNotBottom = hostButtonPosition.top + buttonHeight * buttons.length < document.documentElement.offsetHeight;

      if (topNotBottom)
        buttons = buttons.reverse();

      optionsBar.element.style.height = (buttonHeight * buttons.length) + 'px';
      if (topNotBottom)
        optionsBar.element.style.top = (hostButtonPosition.top + 1) + 'px';
      else
        optionsBar.element.style.top = (hostButtonPosition.top - (buttonHeight * (buttons.length - 1))) + 'px';
      optionsBar.element.style.left = (hostButtonPosition.left + 1) + 'px';

      for (var i = 0; i < buttons.length; ++i) {
        buttons[i].element.addEventListener('mousemove', mouseOver, false);
        buttons[i].element.addEventListener('mouseout', mouseOut, false);
        optionsBar.appendToolbarItem(buttons[i]);
      }
      var hostButtonIndex = topNotBottom ? 0 : buttons.length - 1;
      buttons[hostButtonIndex].element.classList.add('emulate-active');

      function mouseOver(e) {
        if (e.which !== 1)
          return;
        var buttonElement = e.target.enclosingNodeOrSelfWithClass('toolbar-item');
        buttonElement.classList.add('emulate-active');
      }

      function mouseOut(e) {
        if (e.which !== 1)
          return;
        var buttonElement = e.target.enclosingNodeOrSelfWithClass('toolbar-item');
        buttonElement.classList.remove('emulate-active');
      }

      function mouseUp(e) {
        if (e.which !== 1)
          return;
        optionsGlassPane.hide();
        document.documentElement.removeEventListener('mouseup', mouseUp, false);

        for (var i = 0; i < buttons.length; ++i) {
          if (buttons[i].element.classList.contains('emulate-active')) {
            buttons[i].element.classList.remove('emulate-active');
            buttons[i]._clicked(e);
            break;
          }
        }
      }
    }
  }

  /**
   * @param {string} actionId
   * @return {!UI.ToolbarToggle}
   */
  static createActionButtonForId(actionId) {
    const action = UI.actionRegistry.action(actionId);
    return UI.Toolbar.createActionButton(/** @type {!UI.Action} */ (action));
  }

  /**
   * @param {boolean=} reverse
   */
  makeWrappable(reverse) {
    this._contentElement.classList.add('wrappable');
    this._reverse = !!reverse;
    if (reverse)
      this._contentElement.classList.add('wrappable-reverse');
  }

  makeVertical() {
    this._contentElement.classList.add('vertical');
  }

  makeBlueOnHover() {
    this._contentElement.classList.add('toolbar-blue-on-hover');
  }

  makeToggledGray() {
    this._contentElement.classList.add('toolbar-toggled-gray');
  }

  renderAsLinks() {
    this._contentElement.classList.add('toolbar-render-as-links');
  }

  /**
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    this._enabled = enabled;
    for (var item of this._items)
      item._applyEnabledState(this._enabled && item._enabled);
  }

  /**
   * @param {!UI.ToolbarItem} item
   */
  appendToolbarItem(item) {
    this._items.push(item);
    item._toolbar = this;
    if (!this._enabled)
      item._applyEnabledState(false);
    if (this._reverse)
      this._contentElement.insertBefore(item.element, this._insertionPoint.nextSibling);
    else
      this._contentElement.insertBefore(item.element, this._insertionPoint);
    this._hideSeparatorDupes();
  }

  appendSeparator() {
    this.appendToolbarItem(new UI.ToolbarSeparator());
  }

  appendSpacer() {
    this.appendToolbarItem(new UI.ToolbarSeparator(true));
  }

  /**
   * @param {string} text
   */
  appendText(text) {
    this.appendToolbarItem(new UI.ToolbarText(text));
  }

  removeToolbarItems() {
    for (var item of this._items)
      delete item._toolbar;
    this._items = [];
    this._contentElement.removeChildren();
    this._insertionPoint = this._contentElement.createChild('content');
  }

  /**
   * @param {string} color
   */
  setColor(color) {
    var style = createElement('style');
    style.textContent = '.toolbar-glyph { background-color: ' + color + ' !important }';
    this._shadowRoot.appendChild(style);
  }

  /**
   * @param {string} color
   */
  setToggledColor(color) {
    var style = createElement('style');
    style.textContent =
        '.toolbar-button.toolbar-state-on .toolbar-glyph { background-color: ' + color + ' !important }';
    this._shadowRoot.appendChild(style);
  }

  _hideSeparatorDupes() {
    if (!this._items.length)
      return;
    // Don't hide first and last separators if they were added explicitly.
    var previousIsSeparator = false;
    var lastSeparator;
    var nonSeparatorVisible = false;
    for (var i = 0; i < this._items.length; ++i) {
      if (this._items[i] instanceof UI.ToolbarSeparator) {
        this._items[i].setVisible(!previousIsSeparator);
        previousIsSeparator = true;
        lastSeparator = this._items[i];
        continue;
      }
      if (this._items[i].visible()) {
        previousIsSeparator = false;
        lastSeparator = null;
        nonSeparatorVisible = true;
      }
    }
    if (lastSeparator && lastSeparator !== this._items.peekLast())
      lastSeparator.setVisible(false);

    this.element.classList.toggle('hidden', !!lastSeparator && lastSeparator.visible() && !nonSeparatorVisible);
  }

  /**
   * @param {string} location
   */
  appendLocationItems(location) {
    var extensions = self.runtime.extensions(UI.ToolbarItem.Provider);
    var promises = [];
    for (var i = 0; i < extensions.length; ++i) {
      if (extensions[i].descriptor()['location'] === location)
        promises.push(resolveItem(extensions[i]));
    }
    Promise.all(promises).then(appendItemsInOrder.bind(this));

    /**
     * @param {!Runtime.Extension} extension
     * @return {!Promise<?UI.ToolbarItem>}
     */
    function resolveItem(extension) {
      var descriptor = extension.descriptor();
      if (descriptor['separator'])
        return Promise.resolve(/** @type {?UI.ToolbarItem} */ (new UI.ToolbarSeparator()));
      if (descriptor['actionId']) {
        return Promise.resolve(
            /** @type {?UI.ToolbarItem} */ (UI.Toolbar.createActionButtonForId(descriptor['actionId'])));
      }
      return extension.instance().then(fetchItemFromProvider);

      /**
       * @param {!Object} provider
       * @return {?UI.ToolbarItem}
       */
      function fetchItemFromProvider(provider) {
        return /** @type {!UI.ToolbarItem.Provider} */ (provider).item();
      }
    }

    /**
     * @param {!Array.<?UI.ToolbarItem>} items
     * @this {UI.Toolbar}
     */
    function appendItemsInOrder(items) {
      for (var i = 0; i < items.length; ++i) {
        var item = items[i];
        if (item)
          this.appendToolbarItem(item);
      }
    }
  }
};

/**
 * @unrestricted
 */
UI.ToolbarItem = class extends Common.Object {
  /**
   * @param {!Element} element
   */
  constructor(element) {
    super();
    this.element = element;
    this.element.classList.add('toolbar-item');
    this._visible = true;
    this._enabled = true;
    this.element.addEventListener('mouseenter', this._mouseEnter.bind(this), false);
    this.element.addEventListener('mouseleave', this._mouseLeave.bind(this), false);
  }

  /**
   * @param {!Element|string} title
   */
  setTitle(title) {
    if (this._title === title)
      return;
    this._title = title;
    UI.Tooltip.install(this.element, title);
  }

  _mouseEnter() {
    this.element.classList.add('hover');
  }

  _mouseLeave() {
    this.element.classList.remove('hover');
  }

  /**
   * @param {boolean} value
   */
  setEnabled(value) {
    if (this._enabled === value)
      return;
    this._enabled = value;
    this._applyEnabledState(this._enabled && (!this._toolbar || this._toolbar._enabled));
  }

  /**
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    this.element.disabled = !enabled;
  }

  /**
   * @return {boolean} x
   */
  visible() {
    return this._visible;
  }

  /**
   * @param {boolean} x
   */
  setVisible(x) {
    if (this._visible === x)
      return;
    this.element.classList.toggle('hidden', !x);
    this._visible = x;
    if (this._toolbar && !(this instanceof UI.ToolbarSeparator))
      this._toolbar._hideSeparatorDupes();
  }
};

/**
 * @unrestricted
 */
UI.ToolbarText = class extends UI.ToolbarItem {
  /**
   * @param {string=} text
   */
  constructor(text) {
    super(createElementWithClass('div', 'toolbar-text'));
    this.element.classList.add('toolbar-text');
    this.setText(text || '');
  }

  /**
   * @param {string} text
   */
  setText(text) {
    this.element.textContent = text;
  }
};

/**
 * @unrestricted
 */
UI.ToolbarButton = class extends UI.ToolbarItem {
  /**
   * @param {string} title
   * @param {string=} glyph
   * @param {string=} text
   */
  constructor(title, glyph, text) {
    super(createElementWithClass('button', 'toolbar-button'));
    this.element.addEventListener('click', this._clicked.bind(this), false);
    this.element.addEventListener('mousedown', this._mouseDown.bind(this), false);
    this.element.addEventListener('mouseup', this._mouseUp.bind(this), false);

    this._glyphElement = UI.Icon.create('', 'toolbar-glyph hidden');
    this.element.appendChild(this._glyphElement);
    this._textElement = this.element.createChild('div', 'toolbar-text hidden');

    this.setTitle(title);
    if (glyph)
      this.setGlyph(glyph);
    this.setText(text || '');
    this._title = '';
  }

  /**
   * @param {string} text
   */
  setText(text) {
    if (this._text === text)
      return;
    this._textElement.textContent = text;
    this._textElement.classList.toggle('hidden', !text);
    this._text = text;
  }

  /**
   * @param {string} glyph
   */
  setGlyph(glyph) {
    if (this._glyph === glyph)
      return;
    this._glyphElement.setIconType(glyph);
    this._glyphElement.classList.toggle('hidden', !glyph);
    this.element.classList.toggle('toolbar-has-glyph', !!glyph);
    this._glyph = glyph;
  }

  /**
   * @param {string} iconURL
   */
  setBackgroundImage(iconURL) {
    this.element.style.backgroundImage = 'url(' + iconURL + ')';
  }

  /**
   * @param {number=} width
   */
  turnIntoSelect(width) {
    this.element.classList.add('toolbar-has-dropdown');
    var dropdownArrowIcon = UI.Icon.create('smallicon-dropdown-arrow', 'toolbar-dropdown-arrow');
    this.element.appendChild(dropdownArrowIcon);
    if (width)
      this.element.style.width = width + 'px';
  }

  /**
   * @param {!Event} event
   */
  _clicked(event) {
    this.dispatchEventToListeners(UI.ToolbarButton.Events.Click, event);
    event.consume();
  }

  /**
   * @param {!Event} event
   */
  _mouseDown(event) {
    this.dispatchEventToListeners(UI.ToolbarButton.Events.MouseDown, event);
  }

  /**
   * @param {!Event} event
   */
  _mouseUp(event) {
    this.dispatchEventToListeners(UI.ToolbarButton.Events.MouseUp, event);
  }
};

UI.ToolbarButton.Events = {
  Click: Symbol('Click'),
  MouseDown: Symbol('MouseDown'),
  MouseUp: Symbol('MouseUp')
};

UI.ToolbarInput = class extends UI.ToolbarItem {
  /**
   * @param {string} placeholder
   * @param {number=} growFactor
   * @param {number=} shrinkFactor
   * @param {boolean=} isSearchField
   */
  constructor(placeholder, growFactor, shrinkFactor, isSearchField) {
    super(createElementWithClass('div', 'toolbar-input'));

    this.input = this.element.createChild('input');
    this.input.addEventListener('focus', () => this.element.classList.add('focused'));
    this.input.addEventListener('blur', () => this.element.classList.remove('focused'));
    this.input.addEventListener('input', () => this._onChangeCallback(), false);
    this._isSearchField = !!isSearchField;
    if (growFactor)
      this.element.style.flexGrow = growFactor;
    if (shrinkFactor)
      this.element.style.flexShrink = shrinkFactor;
    if (placeholder)
      this.input.setAttribute('placeholder', placeholder);

    if (isSearchField)
      this._setupSearchControls();

    this._updateEmptyStyles();
  }

  _setupSearchControls() {
    var clearButton = this.element.createChild('div', 'toolbar-input-clear-button');
    clearButton.appendChild(UI.Icon.create('smallicon-clear-input', 'search-cancel-button'));
    clearButton.addEventListener('click', () => this._internalSetValue('', true));
    this.input.addEventListener('keydown', event => this._onKeydownCallback(event));
  }

  /**
   * @param {string} value
   */
  setValue(value) {
    this._internalSetValue(value, false);
  }

  /**
   * @param {string} value
   * @param {boolean} notify
   */
  _internalSetValue(value, notify) {
    this.input.value = value;
    if (notify)
      this._onChangeCallback();
    this._updateEmptyStyles();
  }

  /**
   * @return {string}
   */
  value() {
    return this.input.value;
  }

  /**
   * @param {!Event} event
   */
  _onKeydownCallback(event) {
    if (!this._isSearchField || !isEscKey(event) || !this.input.value)
      return;
    this._internalSetValue('', true);
    event.consume(true);
  }

  _onChangeCallback() {
    this._updateEmptyStyles();
    this.dispatchEventToListeners(UI.ToolbarInput.Event.TextChanged, this.input.value);
  }

  _updateEmptyStyles() {
    this.element.classList.toggle('toolbar-input-empty', !this.input.value);
  }
};

UI.ToolbarInput.Event = {
  TextChanged: Symbol('TextChanged')
};

/**
 * @unrestricted
 */
UI.ToolbarToggle = class extends UI.ToolbarButton {
  /**
   * @param {string} title
   * @param {string=} glyph
   * @param {string=} toggledGlyph
   */
  constructor(title, glyph, toggledGlyph) {
    super(title, glyph, '');
    this._toggled = false;
    this._untoggledGlyph = glyph;
    this._toggledGlyph = toggledGlyph;
    this.element.classList.add('toolbar-state-off');
    UI.ARIAUtils.setPressed(this.element, false);
  }

  /**
   * @return {boolean}
   */
  toggled() {
    return this._toggled;
  }

  /**
   * @param {boolean} toggled
   */
  setToggled(toggled) {
    if (this._toggled === toggled)
      return;
    this._toggled = toggled;
    this.element.classList.toggle('toolbar-state-on', toggled);
    this.element.classList.toggle('toolbar-state-off', !toggled);
    UI.ARIAUtils.setPressed(this.element, toggled);
    if (this._toggledGlyph && this._untoggledGlyph)
      this.setGlyph(toggled ? this._toggledGlyph : this._untoggledGlyph);
  }

  /**
   * @param {boolean} withRedColor
   */
  setDefaultWithRedColor(withRedColor) {
    this.element.classList.toggle('toolbar-default-with-red-color', withRedColor);
  }

  /**
   * @param {boolean} toggleWithRedColor
   */
  setToggleWithRedColor(toggleWithRedColor) {
    this.element.classList.toggle('toolbar-toggle-with-red-color', toggleWithRedColor);
  }
};


/**
 * @unrestricted
 */
UI.ToolbarMenuButton = class extends UI.ToolbarButton {
  /**
   * @param {function(!UI.ContextMenu)} contextMenuHandler
   * @param {boolean=} useSoftMenu
   */
  constructor(contextMenuHandler, useSoftMenu) {
    super('', 'largeicon-menu');
    this._contextMenuHandler = contextMenuHandler;
    this._useSoftMenu = !!useSoftMenu;
  }

  /**
   * @override
   * @param {!Event} event
   */
  _mouseDown(event) {
    if (event.buttons !== 1) {
      super._mouseDown(event);
      return;
    }

    if (!this._triggerTimeout)
      this._triggerTimeout = setTimeout(this._trigger.bind(this, event), 200);
  }

  /**
   * @param {!Event} event
   */
  _trigger(event) {
    delete this._triggerTimeout;

    // Throttling avoids entering a bad state on Macs when rapidly triggering context menus just
    // after the window gains focus. See crbug.com/655556
    if (this._lastTriggerTime && Date.now() - this._lastTriggerTime < 300)
      return;
    var contextMenu = new UI.ContextMenu(
        event, this._useSoftMenu, this.element.totalOffsetLeft(),
        this.element.totalOffsetTop() + this.element.offsetHeight);
    this._contextMenuHandler(contextMenu);
    contextMenu.show();
    this._lastTriggerTime = Date.now();
  }

  /**
   * @override
   * @param {!Event} event
   */
  _clicked(event) {
    if (!this._triggerTimeout)
      return;
    clearTimeout(this._triggerTimeout);
    this._trigger(event);
  }
};

/**
 * @unrestricted
 */
UI.ToolbarSettingToggle = class extends UI.ToolbarToggle {
  /**
   * @param {!Common.Setting} setting
   * @param {string} glyph
   * @param {string} title
   * @param {string=} toggledTitle
   */
  constructor(setting, glyph, title, toggledTitle) {
    super(title, glyph);
    this._defaultTitle = title;
    this._toggledTitle = toggledTitle || title;
    this._setting = setting;
    this._settingChanged();
    this._setting.addChangeListener(this._settingChanged, this);
  }

  _settingChanged() {
    var toggled = this._setting.get();
    this.setToggled(toggled);
    this.setTitle(toggled ? this._toggledTitle : this._defaultTitle);
  }

  /**
   * @override
   * @param {!Event} event
   */
  _clicked(event) {
    this._setting.set(!this.toggled());
    super._clicked(event);
  }
};

/**
 * @unrestricted
 */
UI.ToolbarSeparator = class extends UI.ToolbarItem {
  /**
   * @param {boolean=} spacer
   */
  constructor(spacer) {
    super(createElementWithClass('div', spacer ? 'toolbar-spacer' : 'toolbar-divider'));
  }
};

/**
 * @interface
 */
UI.ToolbarItem.Provider = function() {};

UI.ToolbarItem.Provider.prototype = {
  /**
   * @return {?UI.ToolbarItem}
   */
  item() {}
};

/**
 * @interface
 */
UI.ToolbarItem.ItemsProvider = function() {};

UI.ToolbarItem.ItemsProvider.prototype = {
  /**
   * @return {!Array<!UI.ToolbarItem>}
   */
  toolbarItems() {}
};

/**
 * @unrestricted
 */
UI.ToolbarComboBox = class extends UI.ToolbarItem {
  /**
   * @param {?function(!Event)} changeHandler
   * @param {string=} className
   */
  constructor(changeHandler, className) {
    super(createElementWithClass('span', 'toolbar-select-container'));

    this._selectElement = this.element.createChild('select', 'toolbar-item');
    var dropdownArrowIcon = UI.Icon.create('smallicon-dropdown-arrow', 'toolbar-dropdown-arrow');
    this.element.appendChild(dropdownArrowIcon);
    if (changeHandler)
      this._selectElement.addEventListener('change', changeHandler, false);
    if (className)
      this._selectElement.classList.add(className);
  }

  /**
   * @return {!HTMLSelectElement}
   */
  selectElement() {
    return /** @type {!HTMLSelectElement} */ (this._selectElement);
  }

  /**
   * @return {number}
   */
  size() {
    return this._selectElement.childElementCount;
  }

  /**
   * @return {!Array.<!Element>}
   */
  options() {
    return Array.prototype.slice.call(this._selectElement.children, 0);
  }

  /**
   * @param {!Element} option
   */
  addOption(option) {
    this._selectElement.appendChild(option);
  }

  /**
   * @param {string} label
   * @param {string=} title
   * @param {string=} value
   * @return {!Element}
   */
  createOption(label, title, value) {
    var option = this._selectElement.createChild('option');
    option.text = label;
    if (title)
      option.title = title;
    if (typeof value !== 'undefined')
      option.value = value;
    return option;
  }

  /**
   * @override
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    super._applyEnabledState(enabled);
    this._selectElement.disabled = !enabled;
  }

  /**
   * @param {!Element} option
   */
  removeOption(option) {
    this._selectElement.removeChild(option);
  }

  removeOptions() {
    this._selectElement.removeChildren();
  }

  /**
   * @return {?Element}
   */
  selectedOption() {
    if (this._selectElement.selectedIndex >= 0)
      return this._selectElement[this._selectElement.selectedIndex];
    return null;
  }

  /**
   * @param {!Element} option
   */
  select(option) {
    this._selectElement.selectedIndex = Array.prototype.indexOf.call(/** @type {?} */ (this._selectElement), option);
  }

  /**
   * @param {number} index
   */
  setSelectedIndex(index) {
    this._selectElement.selectedIndex = index;
  }

  /**
   * @return {number}
   */
  selectedIndex() {
    return this._selectElement.selectedIndex;
  }

  /**
   * @param {number} width
   */
  setMaxWidth(width) {
    this._selectElement.style.maxWidth = width + 'px';
  }
};

/**
 * @unrestricted
 */
UI.ToolbarSettingComboBox = class extends UI.ToolbarComboBox {
  /**
   * @param {!Array.<!{value: string, label: string, title: string, default:(boolean|undefined)}>} options
   * @param {!Common.Setting} setting
   * @param {string=} optGroup
   */
  constructor(options, setting, optGroup) {
    super(null);
    this._setting = setting;
    this._options = options;
    this._selectElement.addEventListener('change', this._valueChanged.bind(this), false);
    var optionContainer = this._selectElement;
    var optGroupElement = optGroup ? this._selectElement.createChild('optgroup') : null;
    if (optGroupElement) {
      optGroupElement.label = optGroup;
      optionContainer = optGroupElement;
    }
    for (var i = 0; i < options.length; ++i) {
      var dataOption = options[i];
      var option = this.createOption(dataOption.label, dataOption.title, dataOption.value);
      optionContainer.appendChild(option);
      if (setting.get() === dataOption.value)
        this.setSelectedIndex(i);
    }

    setting.addChangeListener(this._settingChanged, this);
  }

  /**
   * @return {string}
   */
  value() {
    return this._options[this.selectedIndex()].value;
  }

  _settingChanged() {
    if (this._muteSettingListener)
      return;

    var value = this._setting.get();
    for (var i = 0; i < this._options.length; ++i) {
      if (value === this._options[i].value) {
        this.setSelectedIndex(i);
        break;
      }
    }
  }

  /**
   * @param {!Event} event
   */
  _valueChanged(event) {
    var option = this._options[this.selectedIndex()];
    this._muteSettingListener = true;
    this._setting.set(option.value);
    this._muteSettingListener = false;
  }
};

/**
 * @unrestricted
 */
UI.ToolbarCheckbox = class extends UI.ToolbarItem {
  /**
   * @param {string} text
   * @param {string=} title
   * @param {!Common.Setting=} setting
   * @param {function()=} listener
   */
  constructor(text, title, setting, listener) {
    super(UI.createCheckboxLabel(text));
    this.element.classList.add('checkbox');
    this.inputElement = this.element.checkboxElement;
    if (title)
      this.element.title = title;
    if (setting)
      UI.SettingsUI.bindCheckbox(this.inputElement, setting);
    if (listener)
      this.inputElement.addEventListener('click', listener, false);
  }

  /**
   * @return {boolean}
   */
  checked() {
    return this.inputElement.checked;
  }

  /**
   * @param {boolean} value
   */
  setChecked(value) {
    this.inputElement.checked = value;
  }

  /**
   * @override
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    super._applyEnabledState(enabled);
    this.inputElement.disabled = !enabled;
  }
};
