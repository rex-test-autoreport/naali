// !ref: local://LoginWebWidget.ui
// !ref: local://LoginWebSettings.ui

engine.ImportExtension("qt.core");
engine.ImportExtension("qt.gui");
engine.ImportExtension("qt.webkit");
engine.ImportExtension("qt.network");

engine.IncludeFile("jsmodules/lib/class.js");
engine.IncludeFile("jsmodules/lib/json2.js");

var uiBase = "./jsmodules/browserui/ui/"
		
var iconRefresh = new QIcon(uiBase + "refresh.png");
var iconStop = new QIcon(uiBase + "stop.png");
var defaultIcon = new QIcon(uiBase + "default-tool.png");

// This magic number should do the trick of keeping the top part
// of the UI same if you are in 3D tab or web tab. Hopefully the linux
// and other operating systems wont get into trouble here (like different font height etc).
// There are minimum sizes for all widgets
var magicHeightValue = 81;

// Browser manager controls the web login widget.
// This object will use private p_ variable in 
// functions that are potentially called from outside.
var BrowserManager = Class.extend
({
    init: function()
    {
        this.connected = false;
        this.squeezeEnabled = false;
        
        this.settings = new BrowserSettings(this);
        this.browserstorage = new BrowserStorage(this);
        this.bookmarks = new BrowserBookmarks(this);
		this.classiclogin = new ClassicLogin();

        // Load widget and init children etc.
        this.browser = ui.LoadFromFile(uiBase + "LoginWebWidget.ui", false);
        this.browser.setWindowState(Qt.WindowFullScreen);
        this.browser.windowFlags = Qt.Widget;
        
        this.browserProxy = ui.AddWidgetToScene(this.browser, Qt.Widget);
        this.browserProxy.windowFlags = Qt.Widget;
        this.browserProxy.effect = 0;
        this.browserProxy.z = 10000;
        
        this.tabs = findChild(this.browser, "tabs");
        this.tabs.clear();
        
        this.mainFrame = findChild(this.browser, "mainFrame");
        if (ui.MainWindow() && ui.MainWindow().menuBar())
            this.mainFrame.layout().setContentsMargins(0,ui.MainWindow().menuBar().height-5,0,0)

        // Load the classic login screen
        this.tabs.addTab(this.classiclogin.widget, "Tundra");
        
        // Kind of a hack on how to put this in its place
        var newTabButton = new QPushButton(this.browser);
        newTabButton.tooltip = "Open New Tab";
        newTabButton.flat = true;
        newTabButton.setFixedSize(16,16);
        newTabButton.setStyleSheet("background-image: url(" + uiBase + "tab-new.png); border: 0px;");
        newTabButton.move(3,60); // Here be dragons
        
        // Address and progress bar
        this.addressBar = new QComboBox();
        this.addressBar.setFixedHeight(23);
        this.addressBar.editable = true;
        this.progressBar = findChild(this.browser, "progressBar");
        this.progressBar.visible = false;
        
        var controlLayout = findChild(this.browser, "controlLayout");
        
        // work around for bug in Anything.prototype.addAction
        QToolBar.prototype.addAction = QWidget.prototype.addAction;
        
        // Browser controls
        this.browserToolBar = new QToolBar();
        this.browserToolBar.setFixedHeight(26);
        this.browserToolBar.setStyleSheet("background-color: none; border: 0px;");
        this.browserToolBar.toolButtonStyle = Qt.ToolButtonIconOnly;
        this.browserToolBar.orientation = Qt.Horizontal;
        this.browserToolBar.iconSize = new QSize(23,23);
        this.browserToolBar.floatable = false;
        this.browserToolBar.movable = false;
        
        this.actionBack = new QAction(new QIcon(uiBase + "back.png"), "", null);
        this.actionBack.triggered.connect(this.onBack);
        this.actionBack.toolTip = "Back";
        this.actionForward = new QAction(new QIcon(uiBase + "forward.png"), "", null);
        this.actionForward.triggered.connect(this.onForward);
        this.actionForward.toolTip = "Forward";
        this.actionRefreshStop = new QAction(new QIcon(uiBase + "refresh.png"), "", null);
        this.actionRefreshStop.triggered.connect(this.onRefreshStop);
        this.actionRefreshStop.tooltip = "Refresh";
        this.actionHome = new QAction(new QIcon(uiBase + "home.png"), "", null);
        this.actionHome.triggered.connect(this.onHome);
        this.actionHome.toolTip = "Go to home page " + this.settings.homepage;
        
        this.browserToolBar.addAction(this.actionBack);
        this.browserToolBar.addAction(this.actionForward);
        this.browserToolBar.addAction(this.actionRefreshStop);
        this.browserToolBar.addAction(this.actionHome);
        
        // Toolbar for inworld actions
        this.toolBarGroups = {};
        this.toolBarContainers = {};
        
        this.toolBar = new QToolBar();
        this.toolBar.setFixedHeight(26);
        this.toolBar.setStyleSheet("background-color: none; border: 0px;");
        this.toolBar.toolButtonStyle = Qt.ToolButtonIconOnly;
        this.toolBar.orientation = Qt.Horizontal;
        this.toolBar.iconSize = new QSize(23,23);
        this.toolBar.floatable = false;
        this.toolBar.movable = false;
        this.toolBar.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed);
        
        // Toolbar for address bar and favorites
        this.favoritesToolBar = new QToolBar();
        this.favoritesToolBar.setFixedHeight(26);
        this.favoritesToolBar.setStyleSheet("background-color: none; border: 0px;");
        this.favoritesToolBar.toolButtonStyle = Qt.ToolButtonIconOnly;
        this.favoritesToolBar.orientation = Qt.Horizontal;
        this.favoritesToolBar.iconSize = new QSize(23,23);
        this.favoritesToolBar.floatable = false;
        this.favoritesToolBar.movable = false;
        this.actionBookmarks = new QAction(new QIcon(uiBase + "bookmarks.png"), "", null);
        this.actionBookmarks.triggered.connect(this.onBookmarksPressed);
        this.actionBookmarks.tooltip = "Bookmarks";
        this.actionAddFavorite = new QAction(new QIcon(uiBase + "favorites.png"), "", null);
        this.actionAddFavorite.triggered.connect(this.onFavoritePressed);
        this.actionAddFavorite.tooltip = "Add as home page or add to bookmarks";
        this.actionSettings = new QAction(new QIcon(uiBase + "settings.png"), "", null);
        this.actionSettings.triggered.connect(this.settings.onSettingsPressed);
        this.actionSettings.tooltip = "Browser settings";
        
        this.favoritesToolBar.addAction(this.actionBookmarks);
        this.favoritesToolBar.addAction(this.actionAddFavorite);
        this.favoritesToolBar.addAction(this.actionSettings);
        
        this.addressAndFavoritesBar = new QWidget();
        this.addressAndFavoritesBar.setFixedHeight(26);
        this.addressAndFavoritesBar.setLayout(new QHBoxLayout());
        this.addressAndFavoritesBar.layout().setContentsMargins(0,0,0,0);
        this.addressAndFavoritesBar.layout().addWidget(this.addressBar, 1, 0);
        this.addressAndFavoritesBar.layout().addWidget(this.favoritesToolBar, 0, 0);

        // Splitter
        this.splitter = new QSplitter(Qt.Horizontal);
        this.splitter.setFixedHeight(26);
        this.splitter.handleWidth = 12;
        this.splitter.childrenCollapsible = true;
        this.splitter.addWidget(this.addressAndFavoritesBar);
        this.splitter.addWidget(this.toolBar);
        this.splitter.setStretchFactor(0, 2);
        this.splitterStartState = this.splitter.saveState();
        
        // Combine ui
        controlLayout.addWidget(this.browserToolBar, 0, 0);
        controlLayout.addWidget(this.splitter, 0, 0);
        
        // Connect signals
        this.addressBar.lineEdit().returnPressed.connect(this.onAddressBarRequest);
        this.tabs.tabCloseRequested.connect(this.onTabCloseRequest);
        this.tabs['currentChanged(int)'].connect(this.onTabIndexChanged);
        
        newTabButton.clicked.connect(this.onTabNewRequest);
        
        client.Connected.connect(this.onConnected);
        client.Disconnected.connect(this.onDisconnected);
        
        browserplugin.ActionAddRequest.connect(this.addTool);
        browserplugin.OpenUrlRequest.connect(this.openUrl);
        
        // Menu entry to show/hide browser ui
        var mainwin = ui.MainWindow();
        var hideshowact = mainwin.AddMenuAction("Browser", "Hide/Show Browser UI");
        hideshowact.triggered.connect(this.toggleVisible);
    },
    
    start: function()
    {
        this.setVisible(true);
        
        if (!this.connected)
            this.connected = client.IsConnected();
        if (this.connected)
        {
            this.tabs.currentIndex = 0;
            this.onTabIndexChanged(this.tabs.currentIndex);
            return;
        }
        
        this.onTabIndexChanged(this.tabs.currentIndex);
        
        if (this.settings.startupLoadHomePage)
        {
            if (HasTundraScheme(this.settings.homepage))
                if (!this.settings.startupConnectToHomePage)
                    return;
            this.openUrl(this.settings.homepage);
        }
    },
    
    setVisible: function(visible)
    {
        this.browser.visible = visible;
    },

    toggleVisible: function() {
        p_.browser.visible = !p_.browser.visible;
    },

    getCurrentWidget: function()
    {
        return p_.tabs.widget(p_.tabs.currentIndex);
    },
    
    addTool: function(action, group)
    {
        if (action.icon.isNull())
            action.icon = defaultIcon;
        if (action.tooltip == null || action.tooltip == "")
            action.tooltip = action.text;

        if (group == null || group == "")
        {
            p_.toolBar.addAction(action);
        }
        else
        {
            var groupToolBar = p_.toolBarGroups[group];
            if (groupToolBar == null)
            {
                var containerWidget = new QWidget();
                containerWidget.setLayout(new QHBoxLayout());
                containerWidget.layout().setSpacing(0);
                containerWidget.layout().setContentsMargins(0,0,0,0);
                
                var nameLabel = new QLabel(group);
                nameLabel.setStyleSheet("color: grey; font: Arial; font-size: 12px;");
                nameLabel.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Expanding);
                nameLabel.alignment = Qt.AlignTop;
                
                containerWidget.layout().addWidget(nameLabel, 0, 0);
                
                groupToolBar = new QToolBar();
                groupToolBar.setFixedHeight(26);
                groupToolBar.setStyleSheet("background-color: none; border: 0px;");
                groupToolBar.toolButtonStyle = Qt.ToolButtonIconOnly;
                groupToolBar.orientation = Qt.Horizontal;
                groupToolBar.iconSize = new QSize(23,23);
                groupToolBar.floatable = false;
                groupToolBar.movable = false;
                groupToolBar.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed);
                containerWidget.layout().addWidget(groupToolBar, 0, 0);
                
                p_.splitter.addWidget(containerWidget);
                
                p_.toolBarGroups[group] = groupToolBar;
                p_.toolBarContainers[group] = containerWidget;
            }
                        
            groupToolBar.addAction(action);
        }
    },
    
    refreshSplitter: function()
    {
        p_.splitter.restoreState(p_.splitterStartState);
    },
    
    refreshSqueezer: function()
    {
        if (this.connected && this.tabs.currentIndex == 0)
        {
            this.browser.maximumHeight = magicHeightValue; // Here be dragons
            this.squeezeEnabled = true;
            this.classiclogin.widget.visible = false;
        }
        else
        {
            this.browser.maximumHeight = 10000;
            this.browser.size = ui.MainWindow().size;
            this.squeezeEnabled = false;
        }
    },
    
    openUrl: function(url, focusNewTab)
    {
        tab = new BrowserTab(p_.tabs, p_.tabCallBack, focusNewTab);
        tab.load(url);
    },

    onConnected: function()
    {
        p_.connected = true;
        p_.tabs.currentIndex = 0;
        p_.refreshSqueezer();
        p_.onTabIndexChanged(p_.tabs.currentIndex);
    },
    
    onDisconnected: function()
    {
        p_.connected = false;
        p_.refreshSqueezer();
        p_.onTabIndexChanged(p_.tabs.currentIndex);
        if (p_.tabs.currentIndex != 0)
        {
            p_.tabs.setTabToolTip(0, "Login");
            p_.tabs.setTabText(0, "Login")
        }
        
        // Clear toolbars
        for (var toolbarKey in p_.toolBarGroups)
        {
            var aToolBar = p_.toolBarGroups[toolbarKey];
            if (aToolBar != null)
            {
                aToolBar.clear();
                aToolBar.deleteLater();
            }
        }
        for (var containerKey in p_.toolBarContainers)
        {
            var container = p_.toolBarContainers[containerKey];
            if (container != null)
                container.deleteLater();
        }
        p_.toolBarGroups = {};
        p_.toolBarContainers = {};
        p_.toolBar.clear();
        p_.refreshSplitter();
    },
    
    onBookmarksPressed: function()
    {
        var bookmarksmenu = new QMenu("Bookmarks", null);
       
        // Tundra world section
        var titleWorlds = bookmarksmenu.addAction("Tundra Worlds");
        var titleFont = titleWorlds.font;
        titleFont.setBold(true);
        titleWorlds.font = titleFont;
        
        var worldmarks = p_.bookmarks.bookmarksworlds;
        if (worldmarks.length > 0)
        {
            for (var i in worldmarks)
            {
                var mark = worldmarks[i];
                var act = bookmarksmenu.addAction(mark.title);
                act.setData(mark.url);
                act.icon = QWebSettings.iconForUrl(new QUrl(mark.url));
            }
        }
        else
            bookmarksmenu.addAction("No items").enabled = false;
        bookmarksmenu.addSeparator();
        
        // Web pages section
        var titleWeb = bookmarksmenu.addAction("Web Pages");
        titleWeb.font = titleFont;
        
        var webmarks = p_.bookmarks.bookmarksweb;
        if (webmarks.length > 0)
        {
            for (var i in webmarks)
            {
                var mark = webmarks[i];
                var act = bookmarksmenu.addAction(mark.title);
                act.setData(mark.url);
                act.icon = QWebSettings.iconForUrl(new QUrl(mark.url));
            }
        }
        else
            bookmarksmenu.addAction("No items").enabled = false;
        
        // Manage bookmarks
        bookmarksmenu.addSeparator();
        var manageAction = bookmarksmenu.addAction("Manage Bookmarks");
        manageAction.font = titleFont;
        manageAction.icon = new QIcon("./data/ui/images/browser/settings.png");
        manageAction.triggered.connect(p_.bookmarks.manageBookmarks);
            
        // Execute menu and act on it
        var clickedAct = bookmarksmenu.exec(QCursor.pos());
        if (clickedAct != null)
        {
            var url = clickedAct.data();
            if (url != null && url != "")
                p_.openUrl(url);
        }
    },
    
    onFavoritePressed: function()
    {
        var addressBarInput = p_.addressBar.lineEdit().text;
        var tabsCurrentIndex = p_.tabs.currentIndex;

        // Validate url for web tabs
        if (tabsCurrentIndex != 0)
        {
            if (p_.getCurrentWidget().url.toString() == "")
            {
                var dialogTitle = "Page load not ready";
                var messageBox = new QMessageBox(QMessageBox.Information, dialogTitle, "Wait for the page to load succesfully first", QMessageBox.NoButton, 0, Qt.Tool);
                messageBox.addButton("OK", QMessageBox.YesRole);
                messageBox.exec();
                return;
            }
        }
        
        var messageBox = new QMessageBox(QMessageBox.NoIcon, 
                                         "Add To Favorites", 
                                         addressBarInput, 
                                         QMessageBox.NoButton,
                                         ui.MainWindow(),
                                         Qt.Tool);
        
        // Disable bookmarks for now
        // \todo Implement bookmarks
        messageBox.addButton("Set As Homepage", QMessageBox.YesRole);
        messageBox.addButton("Add To Bookmarks", QMessageBox.AcceptRole);
        messageBox.addButton("Cancel", QMessageBox.NoRole);
        messageBox.iconPixmap = new QPixmap("./data/ui/images/browser/favorites.png");
        
        var result = messageBox.exec();
        // Return is StandarButton not ButtonRole
        if (result == 0) // QMessageBox.YesRole
        {
            p_.settings.homepage = addressBarInput;
            p_.settings.writeConfig();
        }
        else if (result == 1) // QMessageBox.AcceptRole
        {
            if (tabsCurrentIndex != 0)
                p_.bookmarks.addBookmark(p_.getCurrentWidget().title, p_.getCurrentWidget().url.toString());
            else
            {
                var originalUrl = addressBarInput;
                if (originalUrl.substring(0,9) == "tundra://")
                    originalUrl = originalUrl.substring(9);
            
                // Only keep host and port from world url
                var worldTitle = originalUrl;
                var indexOfFwdSlash = worldTitle.indexOf("/");
                if (indexOfFwdSlash > 0)
                    worldTitle = worldTitle.substring(0, indexOfFwdSlash);
                    
                // Try to resolve username from url and add to title
                var unameStart = originalUrl.indexOf("username=");
                if (unameStart > 0)
                {
                    var unameEnd = originalUrl.indexOf("&", unameStart);
                    if (unameEnd > unameStart)
                    {
                        var username = originalUrl.substring(unameStart + 9, unameEnd);
                        worldTitle = worldTitle + " as " + username;
                    }
                }
                
                p_.bookmarks.addBookmark(worldTitle, addressBarInput);
            }
        }
    },
    
    onAddressBarRequest: function()
    {
        var addressBarInput = p_.addressBar.lineEdit().text;
        var tabsCurrentIndex = p_.tabs.currentIndex;
        if (tabsCurrentIndex == 0)
        {
            if (HasTundraScheme(addressBarInput))
                p_.tabCallBack("weblogin", addressBarInput, tabsCurrentIndex);
            return;
        }
        
        if (!HasTundraScheme(addressBarInput))
        {
            var url = QUrl.fromUserInput(addressBarInput);
            p_.getCurrentWidget().load(url);
        }
        else
        {
            p_.tabCallBack("weblogin", addressBarInput, tabsCurrentIndex);
        }
    },
    
    onTabNewRequest: function()
    {
        if (HasTundraScheme(p_.settings.homepage))
        {
            // This should never happen, settings will prevent you from
            // inserting a tundra:// url as new tab url. But user might
            // set it manually to config so lets still handle the case.
            if (HasTundraScheme(p_.settings.newTabUrl))
            {
                p_.tabCallBack("weblogin", p_.settings.newTabUrl, 0);
                p_.tabs.currentIndex = 0;
            }
            else
                p_.openUrl(p_.settings.newTabUrl);
            return;
        }
            
        if (p_.settings.newTabOpenHomepage)
            p_.openUrl(p_.settings.homepage);
        else
            p_.openUrl(p_.settings.newTabUrl);
    },
    
    onTabIndexChanged: function(index)
    {
        if (index != 0)
        {
            var tab = p_.tabs.widget(index);
            p_.addressBar.lineEdit().text = tab.url.toString();
            p_.actionAddFavorite.enabled = true;
            p_.progressBar.visible = false;
            p_.actionBack.enabled = true;
            p_.actionForward.enabled = true;
            p_.actionRefreshStop.enabled = true;
        }
        else
        {
            p_.progressBar.visible = false;
            p_.actionBack.enabled = false;
            p_.actionForward.enabled = false;
            p_.actionRefreshStop.enabled = false;
            
            if (p_.connected)
            {
                // Login must not end in "/" or it wont look proper
                var loginPropAddress = client.GetLoginProperty("address");
                if (loginPropAddress.charAt(loginPropAddress.length-1) == "/")
                    loginPropAddress = loginPropAddress.substring(0, loginPropAddress.length-1);
                
                // Add all the relevant params we can find from the login properties, skip password though
                var tundraUrl = "tundra://" + loginPropAddress + ":" + client.GetLoginProperty("port");
                tundraUrl = tundraUrl + "/?username=" + client.GetLoginProperty("username");
                if (client.GetLoginProperty("protocol") != "")
                    tundraUrl = tundraUrl + "&protocol=" + client.GetLoginProperty("protocol");
                if (client.GetLoginProperty("avatarurl") != "")
                    tundraUrl = tundraUrl + "&avatarurl=" + client.GetLoginProperty("avatarurl");

                p_.addressBar.lineEdit().text = tundraUrl;
                p_.actionAddFavorite.enabled = true;
                
                p_.tabs.setTabToolTip(0, tundraUrl);
                var tundraUrlShortened = tundraUrl.substring(9);
                if (tundraUrlShortened.length > 23)
                    tundraUrlShortened = tundraUrlShortened.substring(0,20) + "...";
                p_.tabs.setTabText(0, tundraUrlShortened);
            }
            else
            {
                p_.addressBar.lineEdit().text = "local://LoginWidget.ui";
                p_.actionAddFavorite.enabled = false;
                p_.tabs.setTabToolTip(0, "Login");
                p_.tabs.setTabText(0, "Login")
            }
        }
        p_.refreshSqueezer();
    },
    
    onTabCloseRequest: function(index)
    {
        if (index == 0)
            return;
        p_.tabs.widget(index).stop();
        p_.tabs.widget(index).close();
        p_.tabs.widget(index).deleteLater();
        p_.tabs.removeTab(index);
    },
    
    onBack: function()
    {
        p_.getCurrentWidget().back();
    },
    
    onForward: function()
    {
        p_.getCurrentWidget().forward();
    },
    
    onHome: function()
    {
        if (HasTundraScheme(p_.settings.homepage))
        {
            p_.tabCallBack("weblogin", p_.settings.homepage, p_.tabs.currentIndex);
            return;
        }
        
        var qUrl = QUrl.fromUserInput(p_.settings.homepage);
        if (p_.tabs.currentIndex == 0)
        {
            p_.openUrl(qUrl);
        }
        else
        {
            p_.getCurrentWidget().load(qUrl);
        }
    },
    
    onRefreshStop: function()
    {
        var currentTooltip = p_.actionRefreshStop.tooltip;
        if (currentTooltip == "Refresh")
            p_.getCurrentWidget().reload();
        else if (currentTooltip == "Stop")
            p_.getCurrentWidget().stop();
    },

    tabCallBack: function(event, param, index)
    {
        if (event == "started")
        {
            // Never show load bar if squeeze is enabled
            if (!p_.squeezeEnabled) 
                p_.progressBar.visible = true;
            p_.progressBar.value = 0;
            p_.actionRefreshStop.icon = iconStop;
            p_.actionRefreshStop.tooltip = "Stop";
        }
        else if (event == "finished")
        {
            p_.progressBar.visible = false;
            p_.actionRefreshStop.icon = iconRefresh;
            p_.actionRefreshStop.tooltip = "Refresh";
            if (param != "" && p_.tabs.currentIndex == index)
                p_.addressBar.lineEdit().text = param;
        }
        else if (event == "failed")
        {
            p_.progressBar.visible = false;
            p_.actionRefreshStop.icon = iconRefresh;
            p_.actionRefreshStop.tooltip = "Refresh";
        }
        else if (event == "progress")
        {
            // Hide if we are in the 3D/tundra login tab
            if (p_.tabs.currentIndex == 0)
            {
                if (p_.progressBar.visible)
                    p_.progressBar.visible = false;
                return;
            }
            
            // Show and update if we are in the same tab, otherwise hide
            if (p_.tabs.currentIndex == index)
            {
                if (!p_.progressBar.visible)
                    p_.progressBar.visible = true;
                p_.progressBar.value = param;
            }
            else
            {
                if (p_.progressBar.visible)
                    p_.progressBar.visible = false;
            }
        }
        else if (event == "weblogin")
        {
            p_.actionRefreshStop.icon = iconRefresh;
            p_.actionRefreshStop.tooltip = "Refresh";
            
            // We could directly call client.Login(new QUrl(param));
            // but we want to some validatons so we can give and error for the user right here
            // as the login function will just return.
            var loginInfo = ParseTundraLoginInfo(param);
            if (loginInfo["username"] != null && loginInfo["address"] != null)
            {
                if (loginInfo["username"] != "" && loginInfo["address"] != "")
                {
                    // Disconnect if connected
                    if (p_.connected)
                        client.Logout();
                    // Focus 3D tab and close invoking tab
                    if (index != 0)
                    {
                        p_.tabs.currentIndex = 0;
                        p_.tabs.removeTab(index);
                    }
                    p_.loginscreen.visible = false;
                    // Perform login
                    var qLoginUrl = new QUrl.fromUserInput(param);
                    client.Login(qLoginUrl);
                }
            }
        }
    },
});

var BrowserTab = Class.extend
({
    init: function(tabs, callback, focusNewTab)
    {
        this.webview = new QWebView();
        this.webview.page().forwardUnsupportedContent = true;
        this.webview.page().setNetworkAccessManager(p_.browserstorage.accessManager);
        
        this.callback = callback;
        
        this.tabs = tabs;
        this.tabs.addTab(this.webview, "");
        
        this.webview.loadStarted.connect(this, this.loadStarted);
        this.webview.loadFinished.connect(this, this.loadFinished);
        this.webview.loadProgress.connect(this, this.loadProgress);
        this.webview.iconChanged.connect(this, this.iconChanged);
        this.webview.page().unsupportedContent.connect(this, this.unsupportedContent);
        
		if (focusNewTab)
			tabs.currentIndex = this.myIndex();
    },

    myIndex: function()
    {
        return this.tabs.indexOf(this.webview);
    },

    myUrl: function()
    {
        return this.webview.url.toString();
    },

    load: function(url)
    {
        this.webview.load(new QUrl(url))
    },
    
    updateName: function(name)
    {
        this.tabs.setTabToolTip(this.myIndex(), name);
        var name = this.formName(name);
        this.tabs.setTabText(this.myIndex(), name);
    },
    
    formName: function(name)
    {
        if (name.substring(0,7) == "http://")
            name = name.substring(7);
        else if (name.substring(0,8) == "https://")
            name = name.substring(8);
        if (name.charAt(name.length-1) == "/")
            name = name.substring(0,name.length-1);
        if (name.length > 23)
            name = name.substring(0, 20) + "...";
        return name;
    },
    
    loadStarted: function()
    {
        this.updateName("Loading...");
        this.callback("started");
    },
    
    loadProgress: function(step)
    {
        this.callback("progress", step, this.myIndex());
    },
    
    unsupportedContent: function(reply)
    {
        var replyUrl = reply.url().toString();
        if (HasTundraScheme(replyUrl))
            this.callback("weblogin", replyUrl, this.myIndex());
    },
    
    loadFinished: function(success)
    {
        var url = this.myUrl();
        if (success)
        {
            if (url == "about:blank")
            {
                this.updateName("Page Load Error");
                this.callback("finished", "", this.myIndex());
            }
            else
            {
                this.updateName(url);
                this.callback("finished", url, this.myIndex());
            }
        }
        else
        {
            var requestedUrl = this.webview.page().mainFrame().requestedUrl.toString();
            if (HasTundraScheme(requestedUrl))
            {
                this.callback("weblogin", requestedUrl, this.myIndex());
                return;
            }
            
            this.updateName("Page Load Error");
            this.callback("failed");

            // webview.url() is not set to the failed address, we need to change the way
            // BrowserManager call us onAddressBarRequest to call tab.load not webview.load
            //var html = "<p>The page " + url + " could not be loaded...</p>";
            var html = "<p>The page could not be loaded...</p>";
            this.webview.page().mainFrame().setHtml(html);
        }
    },
    
    iconChanged: function()
    {
        this.tabs.setTabIcon(this.myIndex(), this.webview.icon);
    }
});

var BrowserSettings = Class.extend
({
    init: function(browserManager)
    {
        this.browserManager = browserManager;
        
        this.configFile = "browsersettings";
        this.urlSection = "url";
        this.behaviourSection = "behaviour";
        
        this.widget = ui.LoadFromFile(uiBase + "LoginWebSettings.ui", false);
        this.widget.setParent(ui.MainWindow());
        this.widget.setWindowFlags(Qt.Tool);
        this.widget.visible = false;

        var prxyhst = findChild(this.widget, "proxyHostLineEdit");
        var prxyprt = findChild(this.widget, "proxyPortLineEdit");
        var proxycheckbox = findChild(this.widget, "enableProxy");

        prxyhst.setEnabled(proxycheckbox.checked);
        prxyprt.setEnabled(proxycheckbox.checked);

        var button = null;
        button = findChild(this.widget, "buttonSave");
        button.clicked.connect(this.onSettingsSave);
        button = findChild(this.widget, "buttonCancel");
        button.clicked.connect(this.onSettingsCancel);
        proxycheckbox.clicked.connect(this.onSettingsProxyClick);
        
        this.readConfig();
    },
	
    onSettingsProxyClick: function() 
    {
        var p_s = p_.settings;
        var prxyhst = findChild(p_s.widget, "proxyHostLineEdit");       	
        var prxyprt = findChild(p_s.widget, "proxyPortLineEdit");
        var proxycheckbox = findChild(p_s.widget, "enableProxy");
        prxyhst.setEnabled(proxycheckbox.checked);
        prxyprt.setEnabled(proxycheckbox.checked);
    },
   
    onSettingsPressed: function()
    {
        var p_s = p_.settings;
        if (p_s.widget.visible)
        {
            p_s.widget.visible = false;
            return;
        }
        p_s.setCurrentToUi();
        
        var mainWinPosi = ui.MainWindow().pos;
        var mainWinSize = ui.MainWindow().size;
        
        var center_x = mainWinPosi.x() + (mainWinSize.width() / 2);
        var center_y = mainWinPosi.y() + (mainWinSize.height() / 2);       
        
        p_s.widget.pos = new QPoint(center_x - (p_s.widget.width / 2), center_y - (p_s.widget.height / 2));
        p_s.widget.visible = true;
    },
    
    onSettingsSave: function()
    {
        var p_s = p_.settings;
        var child = null;
        
        child = findChild(p_s.widget, "newTabPageLineEdit");
        if (child.text.toLowerCase().substring(0,9) == "tundra://")
        {
            var errorBox = new QMessageBox(QMessageBox.Warning, "Invalid new tab page",
                                             "The new tab url cannot be a tundra:// server", QMessageBox.NoButton,
                                             p_s.widget, Qt.Tool);
            errorBox.addButton("Close", QMessageBox.NoRole);
            errorBox.exec();
            return;
        }
        p_s.newTabUrl = QUrl.fromUserInput(child.text).toString();
        
        child = findChild(p_s.widget, "homePageLineEdit");
        p_s.homepage = QUrl.fromUserInput(child.text).toString();

        child = findChild(p_s.widget, "enableProxy");
        p_s.proxyEnabled = child.checked;

        child = findChild(p_s.widget, "proxyHostLineEdit");
        p_s.proxyhost = child.text;

        if (p_s.proxyhost.search(":") != -1)
        {
            var errorBox = new QMessageBox(QMessageBox.Warning, "Invalid proxy host",
                                           "The proxy host cannot have port, you should put it in to the port field!\nGiven host: " + p_s.proxyhost, 
                                           QMessageBox.NoButton,
                                           p_s.widget, Qt.Tool);
            errorBox.addButton("Close", QMessageBox.NoRole);
            errorBox.exec();
            return;
        }

        child = findChild(p_s.widget, "proxyPortLineEdit");
        p_s.proxyport = child.text;
 
        child = findChild(p_s.widget, "openHomePageOnNewTab");
        p_s.newTabOpenHomepage = child.checked;
        
        child = findChild(p_s.widget, "loadHomePageOnStartup");
        p_s.startupLoadHomePage = child.checked;
        
        child = findChild(p_s.widget, "connectToHomePage");
        p_s.startupConnectToHomePage = child.checked;
        
        child = findChild(p_s.widget, "enableCookies");
        p_s.cookiesEnabled = child.checked;
        
        child = findChild(p_s.widget, "enableCache");
        p_s.cacheEnabled = child.checked;
        
        p_.settings.widget.visible = false;
        p_s.writeConfig();
        p_s.applyProxySettings();
    },
    
    applyProxySettings: function()
    {
        var p_s = this;
        var child = null;
        var proxy = new QNetworkProxy;
    
        if (p_s.proxyEnabled)
        {    
            proxy.setType(QNetworkProxy.HttpProxy);
            proxy.setHostName(p_s.proxyhost);
            proxy.setPort(p_s.proxyport);	
            QNetworkProxy.setApplicationProxy(proxy);
        }
        else 
        {
            proxy.setType(QNetworkProxy.NoProxy);
            QNetworkProxy.setApplicationProxy(proxy);
        }
    },
    
    onSettingsCancel: function()
    {
        p_.settings.widget.visible = false;
    },
    
    setCurrentToUi: function()
    {
        var p_s = p_.settings;
        var child = null;
        
        child = findChild(p_s.widget, "homePageLineEdit");
        child.text = p_s.homepage;

        child = findChild(p_s.widget, "newTabPageLineEdit");
        child.text = p_s.newTabUrl;

        child = findChild(p_s.widget, "proxyHostLineEdit");
        child.text = p_s.proxyhost;

        child = findChild(p_s.widget, "proxyPortLineEdit");
        child.text = p_s.proxyport;
        
        child = findChild(p_s.widget, "openHomePageOnNewTab");
        child.checked = p_s.newTabOpenHomepage;
        
        child = findChild(p_s.widget, "loadHomePageOnStartup");
        child.checked = p_s.startupLoadHomePage;
        
        child = findChild(p_s.widget, "connectToHomePage");
        child.checked = p_s.startupConnectToHomePage;
        
        child = findChild(p_s.widget, "enableCookies");
        child.checked = p_s.cookiesEnabled;
        
        child = findChild(p_s.widget, "enableCache");
        child.checked = p_s.cacheEnabled;
		
        child = findChild(p_s.widget, "enableProxy");
        child.checked = p_s.proxyEnabled;
        p_s.onSettingsProxyClick();
    },
    
    readConfig: function()
    {
        if (!config.HasValue(this.configFile, this.urlSection, "homepage"))
            config.Set(this.configFile, this.urlSection, "homepage", QUrl.fromUserInput("http://login.realxtend.org/").toString());
        this.homepage = config.Get(this.configFile, this.urlSection, "homepage");
        
        if (!config.HasValue(this.configFile, this.urlSection, "newtab"))
            config.Set(this.configFile, this.urlSection, "newtab", QUrl.fromUserInput("http://login.realxtend.org/").toString());
        this.newTabUrl = config.Get(this.configFile, this.urlSection, "newtab");

        if (!config.HasValue(this.configFile, this.urlSection, "proxyhost"))
            config.Set(this.configFile, this.urlSection, "proxyhost", "");
        this.proxyhost = config.Get(this.configFile, this.urlSection, "proxyhost");

        if (!config.HasValue(this.configFile, this.urlSection, "proxyport"))
            config.Set(this.configFile, this.urlSection, "proxyport", "");
        this.proxyport = config.Get(this.configFile, this.urlSection, "proxyport");
        
        // Note: QSettings/QVariant and js booleans dont mix up too well. It will give you a string back of the config value.
        // new Boolean("false") in js will be true, so it cant be used. Inspect the string value and set the booleans right.
        // Also init write will be a string, its a stupid and confusing thing but a work around.
        
        if (!config.HasValue(this.configFile, this.behaviourSection, "newtab_load_homepage"))
            config.Set(this.configFile, this.behaviourSection, "newtab_load_homepage", "false");
        this.newTabOpenHomepage = config.Get(this.configFile, this.behaviourSection, "newtab_load_homepage");
        if (this.newTabOpenHomepage == "true")
            this.newTabOpenHomepage = true;
        else
            this.newTabOpenHomepage = false;
        
        if (!config.HasValue(this.configFile, this.behaviourSection, "startup_load_homepage"))
            config.Set(this.configFile, this.behaviourSection, "startup_load_homepage", "true");
        this.startupLoadHomePage = config.Get(this.configFile, this.behaviourSection, "startup_load_homepage");
        if (this.startupLoadHomePage == "true")
            this.startupLoadHomePage = true;
        else
            this.startupLoadHomePage = false;
        
        if (!config.HasValue(this.configFile, this.behaviourSection, "startup_load_homeserver"))
            config.Set(this.configFile, this.behaviourSection, "startup_load_homeserver", "false");
        this.startupConnectToHomePage = config.Get(this.configFile, this.behaviourSection, "startup_load_homeserver");
        if (this.startupConnectToHomePage == "true")
            this.startupConnectToHomePage = true;
        else
            this.startupConnectToHomePage = false;
            
        if (!config.HasValue(this.configFile, this.behaviourSection, "enable_cookies"))
            config.Set(this.configFile, this.behaviourSection, "enable_cookies", "true");
        this.cookiesEnabled = config.Get(this.configFile, this.behaviourSection, "enable_cookies");
        if (this.cookiesEnabled == "true")
            this.cookiesEnabled = true;
        else
            this.cookiesEnabled = false;
            
        if (!config.HasValue(this.configFile, this.behaviourSection, "enable_cache"))
            config.Set(this.configFile, this.behaviourSection, "enable_cache", "true");
        this.cacheEnabled = config.Get(this.configFile, this.behaviourSection, "enable_cache");
        if (this.cacheEnabled == "true")
            this.cacheEnabled = true;
        else
            this.cacheEnabled = false;
	
        if (!config.HasValue(this.configFile, this.behaviourSection, "enable_proxy"))
            config.Set(this.configFile, this.behaviourSection, "enable_proxy", "false");
        this.proxyEnabled = config.Get(this.configFile, this.behaviourSection, "enable_proxy");
        if (this.proxyEnabled == "true")
            this.proxyEnabled = true;
        else
            this.proxyEnabled = false;
            
        this.applyProxySettings();
    },
    
    writeConfig: function()
    {
        config.Set(this.configFile, this.urlSection, "homepage", this.homepage);
        config.Set(this.configFile, this.urlSection, "newtab", this.newTabUrl);
        config.Set(this.configFile, this.behaviourSection, "newtab_load_homepage", this.newTabOpenHomepage);
        config.Set(this.configFile, this.behaviourSection, "startup_load_homepage", this.startupLoadHomePage);
        config.Set(this.configFile, this.behaviourSection, "startup_load_homeserver", this.startupConnectToHomePage);
        config.Set(this.configFile, this.behaviourSection, "enable_cookies", this.cookiesEnabled);
        config.Set(this.configFile, this.behaviourSection, "enable_cache", this.cacheEnabled);
        config.Set(this.configFile, this.urlSection, "proxyhost", this.proxyhost);
        config.Set(this.configFile, this.urlSection, "proxyport", this.proxyport);
        config.Set(this.configFile, this.behaviourSection, "enable_proxy", this.proxyEnabled);

        this.browserManager.actionHome.toolTip = "Go to home page " + this.homepage;
    },
    
});

var BrowserBookmarks = Class.extend
({
    init: function(browsermanager)
    {
        this.browsermanager = browsermanager;
        this.settings = browsermanager.settings;
        
        this.bookmarksweb = new Array();
        this.bookmarksworlds = new Array();
        
        // Main and list widgets
        this.bookmarkmanager = new QWidget();
        this.bookmarkmanager.setParent(ui.MainWindow());
        this.bookmarkmanager.setWindowTitle("Bookmark Manager");
        this.bookmarkmanager.setWindowFlags(Qt.Tool);
        this.bookmarkmanager.visible = false;
        
        this.listweb = new QListWidget();
        this.listweb.sortingEnabled = false;
        this.listworlds = new QListWidget();
        this.listworlds.sortingEnabled = false;
        
        // Close button
        var closeButton = new QPushButton("Close");
        closeButton.clicked.connect(this.bookmarkmanager.hide);       
        
        // World toolbar
        this.buttonsWorld = new QToolBar();
        this.buttonsWorld.setStyleSheet("background-color: none; border: 0px;");
        this.buttonsWorld.toolButtonStyle = Qt.ToolButtonIconOnly;
        this.buttonsWorld.orientation = Qt.Vertical;
        this.buttonsWorld.iconSize = new QSize(24,24);
        this.buttonsWorld.floatable = false;
        this.buttonsWorld.movable = false;
        
        this.buttonsWorld.addAction(new QIcon(uiBase + "up.png"), "Move Up").triggered.connect(this.moveUpWorld);
        this.buttonsWorld.addAction(new QIcon(uiBase + "down.png"), "Move Down").triggered.connect(this.moveDownWorld);
        this.buttonsWorld.addAction(new QIcon(uiBase + "add.png"), "Add Bookmark").triggered.connect(this.addWorld);
        this.buttonsWorld.addAction(new QIcon(uiBase + "edit.png"), "Edit Bookmark").triggered.connect(this.editWorld);
        this.buttonsWorld.addAction(new QIcon(uiBase + "trash.png"), "Remove Bookmark").triggered.connect(this.removeWorld);
        
        // Web toolbar
        this.buttonsWeb = new QToolBar();
        this.buttonsWeb.setStyleSheet("background-color: none; border: 0px;");
        this.buttonsWeb.toolButtonStyle = Qt.ToolButtonIconOnly;
        this.buttonsWeb.orientation = Qt.Vertical;
        this.buttonsWeb.iconSize = new QSize(24,24);
        this.buttonsWeb.floatable = false;
        this.buttonsWeb.movable = false;
        
        this.buttonsWeb.addAction(new QIcon(uiBase + "up.png"), "Move Up").triggered.connect(this.moveUpWeb);
        this.buttonsWeb.addAction(new QIcon(uiBase + "down.png"), "Move Down").triggered.connect(this.moveDownWeb);
        this.buttonsWeb.addAction(new QIcon(uiBase + "add.png"), "Add Bookmark").triggered.connect(this.addWeb);
        this.buttonsWeb.addAction(new QIcon(uiBase + "edit.png"), "Edit Bookmark").triggered.connect(this.editWeb);
        this.buttonsWeb.addAction(new QIcon(uiBase + "trash.png"), "Remove Bookmark").triggered.connect(this.removeWeb);
               
        // Title labels
        var titleFont = new QFont("Calibri", 12);
        titleFont.setBold(true);
        var titleWorld = new QLabel("World Bookmarks");
        titleWorld.font = titleFont;
        var titleWeb = new QLabel("Web Bookmarks");
        titleWeb.font = titleFont;
        
        // Layouts
        this.worldLayoutHorizontal = new QHBoxLayout();
        this.webLayoutHorizontal = new QHBoxLayout();        
        
        this.bookmarkmanager.setLayout(new QVBoxLayout());
        this.bookmarkmanager.layout().addWidget(titleWorld, 0, 0);
        this.bookmarkmanager.layout().addLayout(this.worldLayoutHorizontal);
        this.bookmarkmanager.layout().addWidget(titleWeb, 0, 0);
        this.bookmarkmanager.layout().addLayout(this.webLayoutHorizontal);
        this.bookmarkmanager.layout().addWidget(closeButton, 0, 0);
        
        this.worldLayoutHorizontal.addWidget(this.listworlds, 1, 0);
        this.worldLayoutHorizontal.addWidget(this.buttonsWorld, 0, 0);
        
        this.webLayoutHorizontal.addWidget(this.listweb, 1, 0);
        this.webLayoutHorizontal.addWidget(this.buttonsWeb, 0, 0);
        
        // Edit widget
        this.editWidget = new QDialog();
        this.editWidget.setParent(ui.MainWindow());
        this.editWidget.setWindowFlags(Qt.Tool);
        this.editWidget.minimumWidth = 350;
        this.editWidget.visible = false;
        this.editWidget.setLayout(new QFormLayout());
        
        this.editDoneButton = new QPushButton("Save");
        this.editDoneButton.clicked.connect(this.editWidget.accept);
        this.editTitleLineEdit = new QLineEdit();
        this.editWidget.layout().addRow("Title", this.editTitleLineEdit);
        this.editUrlLineEdit = new QLineEdit();
        this.editWidget.layout().addRow("URL", this.editUrlLineEdit);
        this.editWidget.layout().addRow("", this.editDoneButton);
        
        this.readBookmarks();
    },
    
    readBookmarks: function()
    {
        if (config.HasValue(this.settings.configFile, "bookmarks", "web"))
        {
            var jsonstringWeb = config.Get(this.settings.configFile, "bookmarks", "web");
            this.bookmarksweb = JSON.parse(jsonstringWeb);
        }
        if (config.HasValue(this.settings.configFile, "bookmarks", "worlds"))
        {
            var jsonstringWorlds = config.Get(this.settings.configFile, "bookmarks", "worlds");
            this.bookmarksworlds = JSON.parse(jsonstringWorlds);
        }
    },
    
    readWorldBookmarksFromUi: function()
    {
        var b = p_.bookmarks;
        b.bookmarksworlds = new Array();
        for (var i=0; i<b.listworlds.count; i++)
        {
            var item = b.listworlds.item(i);
            if (item != null)
                b.bookmarksworlds.push({title:item.text(), url:item.data(Qt.WhatsThisRole)});
        }
        p_.bookmarks.writeWorldBookmarks();
    },
        
    readWebBookmarksFromUi: function()
    {
        var b = p_.bookmarks;
        b.bookmarksweb = new Array();
        for (var i=0; i<b.listweb.count; i++)
        {
            var item = b.listweb.item(i);
            if (item != null)
                b.bookmarksweb.push({title:item.text(), url:item.data(Qt.WhatsThisRole)});
        }
        p_.bookmarks.writeWebBookmarks();
    },
    
    writeWorldBookmarks: function()
    {
        if (this.bookmarksworlds.length > 0)
        {
            var jsonstringWorlds = JSON.stringify(this.bookmarksworlds);
            config.Set(this.settings.configFile, "bookmarks", "worlds", jsonstringWorlds);
        }
    },
    
    writeWebBookmarks: function()
    {
        if (this.bookmarksweb.length > 0)
        {
            var jsonstringWeb = JSON.stringify(this.bookmarksweb);
            config.Set(this.settings.configFile, "bookmarks", "web", jsonstringWeb);
        }
    },
       
    addBookmark: function(title, urlString)
    {
        if (HasTundraScheme(urlString))
        {
            var moddedTitle = QInputDialog.getText(ui.MainWindow(), "Add World Bookmark", "Title of the world bookmark", QLineEdit.Normal, title, Qt.Tool);
            if (moddedTitle != null && moddedTitle != "")
            {
                p_.bookmarks.bookmarksworlds.push({title:moddedTitle, url:urlString});
                p_.bookmarks.writeWorldBookmarks();
            }
        }
        else
        {
            var moddedTitle = QInputDialog.getText(ui.MainWindow(), "Add Web Bookmark", "Title of the web bookmark", QLineEdit.Normal, title, Qt.Tool);
            if (moddedTitle != null && moddedTitle != "")
            {
                p_.bookmarks.bookmarksweb.push({title:moddedTitle, url:urlString});
                p_.bookmarks.writeWebBookmarks();
            }
        }
    },
    
    manageBookmarks: function()
    {        
        // QListWidgetItem
        var w = p_.bookmarks.bookmarkmanager;
        
        // Calculate center pos on our main window
        var mainWinPosi = ui.MainWindow().pos;
        var mainWinSize = ui.MainWindow().size;
        var center_x = mainWinPosi.x() + (mainWinSize.width() / 2);
        var center_y = mainWinPosi.y() + (mainWinSize.height() / 2);       

        // Populate list views
        p_.bookmarks.listworlds.clear();
        p_.bookmarks.listweb.clear();
        
        var worldmarks = p_.bookmarks.bookmarksworlds;
        for (var i in worldmarks)
        {
            var mark = worldmarks[i];
            var item = new QListWidgetItem(mark.title);
            item.setData(Qt.WhatsThisRole, mark.url);
            item.setIcon(QWebSettings.iconForUrl(new QUrl(mark.url)));
            
            p_.bookmarks.listworlds.addItem(item);
        }
        
        var webmarks = p_.bookmarks.bookmarksweb;
        for (var i in webmarks)
        {
            var mark = webmarks[i];
            var item = new QListWidgetItem(mark.title);
            item.setData(Qt.WhatsThisRole, mark.url);
            item.setIcon(QWebSettings.iconForUrl(new QUrl(mark.url)));
            
            p_.bookmarks.listweb.addItem(item);
        }
        
        w.pos = new QPoint(center_x - (w.width / 2), center_y - (w.height / 2));
        w.visible = true;
    },
    
    moveUpWorld: function()
    {
        var l = p_.bookmarks.listworlds;
        var rowNow = l.currentRow;
        if (rowNow >= 0)
        {
            var item = l.takeItem(rowNow);
            l.insertItem(rowNow-1, item);
            l.currentRow = rowNow-1;
        }
        p_.bookmarks.readWorldBookmarksFromUi();
    },
    
    moveDownWorld: function()
    {
        var l = p_.bookmarks.listworlds;
        var rowNow = l.currentRow;
        if (rowNow >= 0)
        {
            var item = l.takeItem(rowNow);
            l.insertItem(rowNow+1, item);
            l.currentRow = rowNow+1;
        }
        p_.bookmarks.readWorldBookmarksFromUi();
    },
    
    addWorld: function()
    {
        var b = p_.bookmarks;
        var l = b.listworlds;
                
        b.editTitleLineEdit.text = "";
        b.editUrlLineEdit.text = "tundra://";
        b.editWidget.setWindowTitle("Add World Bookmark");
        if (b.editWidget.exec() == 1)
        {
            var item = new QListWidgetItem(b.editTitleLineEdit.text);
            item.setData(Qt.WhatsThisRole, QUrl.fromUserInput(b.editUrlLineEdit.text).toString());
            item.setIcon(QWebSettings.iconForUrl(new QUrl(b.editUrlLineEdit.text)));
            l.addItem(item);
        }
        p_.bookmarks.readWorldBookmarksFromUi();
    },
    
    editWorld: function()
    {
        var b = p_.bookmarks;
        var l = b.listworlds;
        
        var item = l.currentItem();
        if (item == null)
            return;
        b.editTitleLineEdit.text = item.text();
        b.editUrlLineEdit.text = item.data(Qt.WhatsThisRole);
        b.editWidget.setWindowTitle("Edit World Bookmark");
        if (b.editWidget.exec() == 1)
        {
            item.setText(b.editTitleLineEdit.text);
            item.setData(Qt.WhatsThisRole, QUrl.fromUserInput(b.editUrlLineEdit.text).toString());
        }
        p_.bookmarks.readWorldBookmarksFromUi();
    },
    
    removeWorld: function()
    {
        var l = p_.bookmarks.listworlds;
        l.takeItem(l.currentRow);
        p_.bookmarks.readWorldBookmarksFromUi();
    },
    
    moveUpWeb: function()
    {
        var l = p_.bookmarks.listweb;
        var rowNow = l.currentRow;
        if (rowNow >= 0)
        {
            var item = l.takeItem(rowNow);
            l.insertItem(rowNow-1, item);
            l.currentRow = rowNow-1;
        }
        p_.bookmarks.readWebBookmarksFromUi();
    },
    
    moveDownWeb: function()
    {
        var l = p_.bookmarks.listweb;
        var rowNow = l.currentRow;
        if (rowNow >= 0)
        {
            var item = l.takeItem(rowNow);
            l.insertItem(rowNow+1, item);
            l.currentRow = rowNow+1;
        }
        p_.bookmarks.readWebBookmarksFromUi();
    },
    
    addWeb: function()
    {
        var b = p_.bookmarks;
        var l = b.listweb;
        
        b.editTitleLineEdit.text = "";
        b.editUrlLineEdit.text = "http://";
        b.editWidget.setWindowTitle("Add Web Bookmark");
        if (b.editWidget.exec() == 1)
        {
            var item = new QListWidgetItem(b.editTitleLineEdit.text);
            item.setData(Qt.WhatsThisRole, QUrl.fromUserInput(b.editUrlLineEdit.text).toString());
            item.setIcon(QWebSettings.iconForUrl(new QUrl(b.editUrlLineEdit.text)));
            l.addItem(item);
        }
        p_.bookmarks.readWebBookmarksFromUi();
    },
    
    editWeb: function()
    {
        var b = p_.bookmarks;
        var l = b.listweb;
        
        var item = l.currentItem();
        if (item == null)
            return;
        b.editTitleLineEdit.text = item.text();
        b.editUrlLineEdit.text = item.data(Qt.WhatsThisRole);
        b.editWidget.setWindowTitle("Edit Web Bookmark");
        if (b.editWidget.exec() == 1)
        {
            item.setText(b.editTitleLineEdit.text);
            item.setData(Qt.WhatsThisRole, QUrl.fromUserInput(b.editUrlLineEdit.text).toString());
        }
        p_.bookmarks.readWebBookmarksFromUi();
    },
    
    removeWeb: function()
    {
        var l = p_.bookmarks.listweb;
        l.takeItem(l.currentRow);
        p_.bookmarks.readWebBookmarksFromUi();
    }
    
});

// Disabling/enabling cache from UI requires a restart of the viewer to take effect. Qt docs explains more:
// "Note: It is currently not supported to change the network access manager after the QWebPage has used it. The results of doing this are undefined."

var BrowserStorage = Class.extend
({
    init: function(browsermanager)
    {
        this.browsermanager = browsermanager;
        this.settings = browsermanager.settings;
        this.enabled = true;
        if (!this.initFolders())
            return;
               
        // Initialise cache objects
        try {
            this.cache = new QNetworkDiskCache(null);
            this.cache.setCacheDirectory(this.cacheDataDir);
            this.accessManager = new QNetworkAccessManager(null);
            this.cookieJar = browserplugin.CreateCookieJar(this.cookieDataFile);
            this.have_cache = true;
        } catch (err) {
            print("Cache init failed: " + err);
            this.have_cache = false;
        }
        // Initialize cache items to our access manager.
        if (this.have_cache && this.settings.cacheEnabled)
        {
            this.accessManager.setCache(this.cache);
            QWebSettings.setIconDatabasePath(this.iconDataDir);
        }
        if (this.have_cache && this.settings.cookiesEnabled)
        {
            this.accessManager.setCookieJar(this.cookieJar);
        }
        
        // Connect to clear buttons
        var button = findChild(this.settings.widget, "clearCookies");
        if (button != null)
            button.clicked.connect(this.clearCookies);
        button = findChild(this.settings.widget, "clearCache");
        if (button != null)
            button.clicked.connect(this.clearCache);
    },
    
    initFolders: function()
    {
        // \todo This is a hack because framework.platform is not exposed to js.
        // So we can't resolve the data dir correctly from there, change this code once its exposed.
        var folderToFind = "browsercache";
        var browserDataDir = new QDir(asset.GetAssetCache().GetCacheDirectory()); // <data_dir>/assetcache/data
        browserDataDir.cdUp(); // data
        browserDataDir.cdUp(); // assetcache
        if (!browserDataDir.exists(folderToFind))
            browserDataDir.mkdir(folderToFind);
        if (!browserDataDir.cd(folderToFind))
        {
            console.LogError("Could not resolve browser data dir, disabling cache and cookies.");
            this.enabled = false;
            return false;
        }
        
        this.browserDataPath = browserDataDir.absolutePath();
        this.cacheDataDir = browserDataDir.absolutePath();
        this.iconDataDir = browserDataDir.absolutePath();
        if (this.settings.cookiesEnabled)
            this.cookieDataFile = browserDataDir.absoluteFilePath("cookies.data");
        else 
            this.cookieDataFile = "";
        return true;
    },

    clearCache: function()
    {
        QWebSettings.clearIconDatabase();
        QWebSettings.clearMemoryCaches();
        p_.browserstorage.cache.clear();
    },
    
    clearCookies: function()
    {
        p_.browserstorage.cookieJar.ClearCookies();
    }
 
});

var ClassicLogin = Class.extend
({
    init: function()
    {
		this.configFile = "tundra";
		this.configSection = "client";

		this.widget = new QWidget();
		this.widget.setLayout(new QVBoxLayout());
		this.widget.layout().setContentsMargins(0,0,0,0);

		var child = ui.LoadFromFile(uiBase + "LoginWidget.ui", false);
		child.setParent(this.widget);
		this.widget.layout().addWidget(child, 0, 0);

		this.loginButton = findChild(this.widget, "pushButton_Connect");
		this.exitButton = findChild(this.widget, "pushButton_Exit");
		this.serverAddressLineEdit = findChild(this.widget, "lineEdit_WorldAddress");
		this.usernameLineEdit = findChild(this.widget, "lineEdit_Username");
		this.passwordLineEdit = findChild(this.widget, "lineEdit_Password");
		this.tcpButton = findChild(this.widget, "radioButton_ProtocolTCP");
		this.udpButton = findChild(this.widget, "radioButton_ProtocolUDP");

		var logoLabel = findChild(this.widget, "label_ClientLogo");
		logoLabel.pixmap = new QPixmap("./data/ui/images/realxtend_logo.png");

		// Connections
		this.loginButton.clicked.connect(this, this.loginPressed);
		this.exitButton.clicked.connect(this, this.exit);
		client.Connected.connect(this, this.onConnected);
		client.Disconnected.connect(this, this.onDisconnected);

		this.readConfigToUi();
	},
	
	readConfigToUi: function()
	{
	    // Make double sure with "" default value and null 
		// checks that we can in any case insert var to ui
		configServer = framework.Config().Get(this.configFile, this.configSection, "login_server", "");
		if (configServer == null)
			configServer = "";
		configUsername = framework.Config().Get(this.configFile, this.configSection, "login_username", "");
		if (configUsername == null)
			configUsername = "";
		configProtocol = framework.Config().Get(this.configFile, this.configSection, "login_protocol", "");
		if (configProtocol == null)
			configProtocol = "";

		this.serverAddressLineEdit.text = configServer;
		this.usernameLineEdit.text = configUsername;
		if (configProtocol == "tcp")
			this.tcpButton.checked = true;
		else if (configProtocol == "udp")
			this.udpButton.checked = true;
	},
		
	writeConfigFromUi: function() 
	{
		// Downside of this is that user may do something to the UI elements while loggin in.
		// In Tundra its so fast that doubt that will happen. Can be fixed to read in to configX values in LoginPresse()
		framework.Config().Set(this.configFile, this.configSection, "login_server", this.trimField(this.serverAddressLineEdit.text));
		framework.Config().Set(this.configFile, this.configSection, "login_username", this.trimField(this.usernameLineEdit.text));
		framework.Config().Set(this.configFile, this.configSection, "login_protocol", this.getProtocol());
	},

	trimField: function(text) 
	{
		return text.replace(/^\s+|\s+$/g, "");
	},

	getProtocol: function() 
	{
		if (this.tcpButton.checked)
			return "tcp";
		else if (this.udpButton.checked)
			return "udp";
		return "";
	},

	loginPressed: function()
	{
		client.ClearLoginProperties();

		var username = this.trimField(this.usernameLineEdit.text);
		var password = this.trimField(this.passwordLineEdit.text);
		var protocol = this.getProtocol();
		if (protocol == "")
			return;
		
		var port = 2345;
		var hostAndPort = this.trimField(this.serverAddressLineEdit.text).split(':');
		if (hostAndPort.length < 1)
		{
			console.LogError("You have to give host to login!");
			return;
		}
		if (hostAndPort.length > 1)
			port = parseInt(hostAndPort[1]);

		client.Login(hostAndPort[0], port, username, password, protocol);
	},

	onConnected: function()
	{
		this.widget.setVisible(false);
		this.writeConfigFromUi();
	},

	onDisconnected: function()
	{
		this.widget.setVisible(true);
	},

	exit: function() 
	{
		framework.Exit();
	}
});

// Common utility functions

function HasTundraScheme(urlString)
{
    if (urlString.toLowerCase().substring(0,9) == "tundra://")
        return true;
    return false;
}

function ParseTundraLoginInfo(urlString)
{
    var loginUrl = new QUrl.fromUserInput(urlString);   
    var loginUsername = loginUrl.queryItemValue("username");
    var loginPassword = loginUrl.queryItemValue("password");
    var loginAvatarurl = loginUrl.queryItemValue("avatarurl");
    var loginProtocol = loginUrl.queryItemValue("protocol");
    var loginAddress = loginUrl.host();
    var loginPort = loginUrl.port();
    if (loginPort == -1)
        loginPort = 2345;
    if (loginProtocol == "" || loginProtocol == null)
        loginProtocol = "tcp";
        
    var loginInfo = { "username" : loginUsername,
                      "password" : loginPassword,
                      "avatarurl" : loginAvatarurl,
                      "protocol" : loginProtocol,
                      "address" : loginAddress,
                      "port" : loginPort };
    return loginInfo;
}

// Startup

if (!server.IsAboutToStart() && !framework.IsHeadless())
{
    var p_ = new BrowserManager();
    p_.start();
}
