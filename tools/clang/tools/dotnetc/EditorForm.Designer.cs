///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// EditorForm.Designer..cs                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

namespace MainNs
{
    partial class EditorForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.TheStatusStrip = new System.Windows.Forms.StatusStrip();
            this.TheStatusStripLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.TheMenuStrip = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.NewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.saveToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.saveAsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.recentFilesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem4 = new System.Windows.Forms.ToolStripSeparator();
            this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.editToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.undoToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripSeparator();
            this.cutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.copyToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.pasteToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.deleteToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem2 = new System.Windows.Forms.ToolStripSeparator();
            this.selectAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem3 = new System.Windows.Forms.ToolStripSeparator();
            this.findAndReplaceToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.quickFindToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.goToToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.fileVariablesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.FontGrowToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.FontShrinkToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.viewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.autoUpdateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.bitstreamToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.bitstreamFromClipboardToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.ColorMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.debugInformationToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.errorListToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.renderToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.outputToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.buildToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.compileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exportCompiledObjectToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.optionsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.rewriterToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.rewriteNobodyToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.CodeBox = new System.Windows.Forms.RichTextBox();
            this.AnalysisTabControl = new System.Windows.Forms.TabControl();
            this.CompilationTabPage = new System.Windows.Forms.TabPage();
            this.btnCompile = new System.Windows.Forms.Button();
            this.tbOptions = new System.Windows.Forms.TextBox();
            this.label6 = new System.Windows.Forms.Label();
            this.cbProfile = new System.Windows.Forms.ComboBox();
            this.label5 = new System.Windows.Forms.Label();
            this.tbEntry = new System.Windows.Forms.TextBox();
            this.label4 = new System.Windows.Forms.Label();
            this.DisassemblyTabPage = new System.Windows.Forms.TabPage();
            this.DisassemblyTextBox = new System.Windows.Forms.RichTextBox();
            this.ASTTabPage = new System.Windows.Forms.TabPage();
            this.ASTDumpBox = new System.Windows.Forms.RichTextBox();
            this.OptimizerTabPage = new System.Windows.Forms.TabPage();
            this.InteractiveEditorButton = new System.Windows.Forms.Button();
            this.ResetDefaultPassesButton = new System.Windows.Forms.Button();
            this.AnalyzeCheckBox = new System.Windows.Forms.CheckBox();
            this.AddPrintModuleButton = new System.Windows.Forms.Button();
            this.RunPassesButton = new System.Windows.Forms.Button();
            this.SelectPassDownButton = new System.Windows.Forms.Button();
            this.SelectPassUpButton = new System.Windows.Forms.Button();
            this.SelectedPassesBox = new System.Windows.Forms.ListBox();
            this.PassesContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.PassPropertiesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem5 = new System.Windows.Forms.ToolStripSeparator();
            this.copyToolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.copyAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.PastePassesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.DeleteAllPassesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.AvailablePassesBox = new System.Windows.Forms.ListBox();
            this.TheToolTip = new System.Windows.Forms.ToolTip(this.components);
            this.TopSplitContainer = new System.Windows.Forms.SplitContainer();
            this.OutputTabControl = new System.Windows.Forms.TabControl();
            this.RenderLogTabPage = new System.Windows.Forms.TabPage();
            this.RenderLogBox = new System.Windows.Forms.TextBox();
            this.RewriterOutputTextBox = new System.Windows.Forms.RichTextBox();
            this.TheStatusStrip.SuspendLayout();
            this.TheMenuStrip.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.AnalysisTabControl.SuspendLayout();
            this.CompilationTabPage.SuspendLayout();
            this.DisassemblyTabPage.SuspendLayout();
            this.ASTTabPage.SuspendLayout();
            this.OptimizerTabPage.SuspendLayout();
            this.PassesContextMenu.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.TopSplitContainer)).BeginInit();
            this.TopSplitContainer.Panel1.SuspendLayout();
            this.TopSplitContainer.Panel2.SuspendLayout();
            this.TopSplitContainer.SuspendLayout();
            this.OutputTabControl.SuspendLayout();
            this.RenderLogTabPage.SuspendLayout();
            this.SuspendLayout();
            // 
            // TheStatusStrip
            // 
            this.TheStatusStrip.ImageScalingSize = new System.Drawing.Size(24, 24);
            this.TheStatusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TheStatusStripLabel});
            this.TheStatusStrip.Location = new System.Drawing.Point(0, 292);
            this.TheStatusStrip.Name = "TheStatusStrip";
            this.TheStatusStrip.Padding = new System.Windows.Forms.Padding(3, 0, 12, 0);
            this.TheStatusStrip.Size = new System.Drawing.Size(902, 30);
            this.TheStatusStrip.TabIndex = 0;
            this.TheStatusStrip.Text = "statusStrip1";
            // 
            // TheStatusStripLabel
            // 
            this.TheStatusStripLabel.Name = "TheStatusStripLabel";
            this.TheStatusStripLabel.Size = new System.Drawing.Size(64, 25);
            this.TheStatusStripLabel.Text = "Ready.";
            // 
            // TheMenuStrip
            // 
            this.TheMenuStrip.ImageScalingSize = new System.Drawing.Size(24, 24);
            this.TheMenuStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.editToolStripMenuItem,
            this.viewToolStripMenuItem,
            this.buildToolStripMenuItem,
            this.toolsToolStripMenuItem,
            this.helpToolStripMenuItem});
            this.TheMenuStrip.Location = new System.Drawing.Point(0, 0);
            this.TheMenuStrip.Name = "TheMenuStrip";
            this.TheMenuStrip.Padding = new System.Windows.Forms.Padding(4, 2, 0, 2);
            this.TheMenuStrip.Size = new System.Drawing.Size(902, 33);
            this.TheMenuStrip.TabIndex = 1;
            this.TheMenuStrip.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.NewToolStripMenuItem,
            this.openToolStripMenuItem,
            this.saveToolStripMenuItem,
            this.saveAsToolStripMenuItem,
            this.toolStripSeparator1,
            this.recentFilesToolStripMenuItem,
            this.toolStripMenuItem4,
            this.exitToolStripMenuItem});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(50, 29);
            this.fileToolStripMenuItem.Text = "&File";
            this.fileToolStripMenuItem.DropDownOpening += new System.EventHandler(this.fileToolStripMenuItem_DropDownOpening);
            // 
            // NewToolStripMenuItem
            // 
            this.NewToolStripMenuItem.Name = "NewToolStripMenuItem";
            this.NewToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.N)));
            this.NewToolStripMenuItem.Size = new System.Drawing.Size(217, 30);
            this.NewToolStripMenuItem.Text = "&New";
            this.NewToolStripMenuItem.Click += new System.EventHandler(this.NewToolStripMenuItem_Click);
            // 
            // openToolStripMenuItem
            // 
            this.openToolStripMenuItem.Name = "openToolStripMenuItem";
            this.openToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.O)));
            this.openToolStripMenuItem.Size = new System.Drawing.Size(217, 30);
            this.openToolStripMenuItem.Text = "&Open...";
            this.openToolStripMenuItem.Click += new System.EventHandler(this.openToolStripMenuItem_Click);
            // 
            // saveToolStripMenuItem
            // 
            this.saveToolStripMenuItem.Name = "saveToolStripMenuItem";
            this.saveToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.S)));
            this.saveToolStripMenuItem.Size = new System.Drawing.Size(217, 30);
            this.saveToolStripMenuItem.Text = "&Save";
            this.saveToolStripMenuItem.Click += new System.EventHandler(this.saveToolStripMenuItem_Click);
            // 
            // saveAsToolStripMenuItem
            // 
            this.saveAsToolStripMenuItem.Name = "saveAsToolStripMenuItem";
            this.saveAsToolStripMenuItem.Size = new System.Drawing.Size(217, 30);
            this.saveAsToolStripMenuItem.Text = "Save &As...";
            this.saveAsToolStripMenuItem.Click += new System.EventHandler(this.saveAsToolStripMenuItem_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(214, 6);
            // 
            // recentFilesToolStripMenuItem
            // 
            this.recentFilesToolStripMenuItem.Name = "recentFilesToolStripMenuItem";
            this.recentFilesToolStripMenuItem.Size = new System.Drawing.Size(217, 30);
            this.recentFilesToolStripMenuItem.Text = "Recent &Files";
            // 
            // toolStripMenuItem4
            // 
            this.toolStripMenuItem4.Name = "toolStripMenuItem4";
            this.toolStripMenuItem4.Size = new System.Drawing.Size(214, 6);
            // 
            // exitToolStripMenuItem
            // 
            this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
            this.exitToolStripMenuItem.Size = new System.Drawing.Size(217, 30);
            this.exitToolStripMenuItem.Text = "E&xit";
            this.exitToolStripMenuItem.Click += new System.EventHandler(this.exitToolStripMenuItem_Click);
            // 
            // editToolStripMenuItem
            // 
            this.editToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.undoToolStripMenuItem,
            this.toolStripMenuItem1,
            this.cutToolStripMenuItem,
            this.copyToolStripMenuItem,
            this.pasteToolStripMenuItem,
            this.deleteToolStripMenuItem,
            this.toolStripMenuItem2,
            this.selectAllToolStripMenuItem,
            this.toolStripMenuItem3,
            this.findAndReplaceToolStripMenuItem,
            this.goToToolStripMenuItem,
            this.fileVariablesToolStripMenuItem,
            this.FontGrowToolStripMenuItem,
            this.FontShrinkToolStripMenuItem});
            this.editToolStripMenuItem.Name = "editToolStripMenuItem";
            this.editToolStripMenuItem.Size = new System.Drawing.Size(54, 29);
            this.editToolStripMenuItem.Text = "&Edit";
            // 
            // undoToolStripMenuItem
            // 
            this.undoToolStripMenuItem.Name = "undoToolStripMenuItem";
            this.undoToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Z)));
            this.undoToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.undoToolStripMenuItem.Text = "&Undo";
            this.undoToolStripMenuItem.Click += new System.EventHandler(this.undoToolStripMenuItem_Click);
            // 
            // toolStripMenuItem1
            // 
            this.toolStripMenuItem1.Name = "toolStripMenuItem1";
            this.toolStripMenuItem1.Size = new System.Drawing.Size(227, 6);
            // 
            // cutToolStripMenuItem
            // 
            this.cutToolStripMenuItem.Name = "cutToolStripMenuItem";
            this.cutToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.X)));
            this.cutToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.cutToolStripMenuItem.Text = "Cu&t";
            this.cutToolStripMenuItem.Click += new System.EventHandler(this.cutToolStripMenuItem_Click);
            // 
            // copyToolStripMenuItem
            // 
            this.copyToolStripMenuItem.Name = "copyToolStripMenuItem";
            this.copyToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.C)));
            this.copyToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.copyToolStripMenuItem.Text = "&Copy";
            this.copyToolStripMenuItem.Click += new System.EventHandler(this.copyToolStripMenuItem_Click);
            // 
            // pasteToolStripMenuItem
            // 
            this.pasteToolStripMenuItem.Name = "pasteToolStripMenuItem";
            this.pasteToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.V)));
            this.pasteToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.pasteToolStripMenuItem.Text = "&Paste";
            this.pasteToolStripMenuItem.Click += new System.EventHandler(this.pasteToolStripMenuItem_Click);
            // 
            // deleteToolStripMenuItem
            // 
            this.deleteToolStripMenuItem.Name = "deleteToolStripMenuItem";
            this.deleteToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.deleteToolStripMenuItem.Text = "&Delete";
            this.deleteToolStripMenuItem.Click += new System.EventHandler(this.deleteToolStripMenuItem_Click);
            // 
            // toolStripMenuItem2
            // 
            this.toolStripMenuItem2.Name = "toolStripMenuItem2";
            this.toolStripMenuItem2.Size = new System.Drawing.Size(227, 6);
            // 
            // selectAllToolStripMenuItem
            // 
            this.selectAllToolStripMenuItem.Name = "selectAllToolStripMenuItem";
            this.selectAllToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.A)));
            this.selectAllToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.selectAllToolStripMenuItem.Text = "Select &All";
            this.selectAllToolStripMenuItem.Click += new System.EventHandler(this.selectAllToolStripMenuItem_Click);
            // 
            // toolStripMenuItem3
            // 
            this.toolStripMenuItem3.Name = "toolStripMenuItem3";
            this.toolStripMenuItem3.Size = new System.Drawing.Size(227, 6);
            // 
            // findAndReplaceToolStripMenuItem
            // 
            this.findAndReplaceToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.quickFindToolStripMenuItem});
            this.findAndReplaceToolStripMenuItem.Name = "findAndReplaceToolStripMenuItem";
            this.findAndReplaceToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.findAndReplaceToolStripMenuItem.Text = "&Find and Replace";
            // 
            // quickFindToolStripMenuItem
            // 
            this.quickFindToolStripMenuItem.Name = "quickFindToolStripMenuItem";
            this.quickFindToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.F)));
            this.quickFindToolStripMenuItem.Size = new System.Drawing.Size(240, 30);
            this.quickFindToolStripMenuItem.Text = "Quick &Find";
            this.quickFindToolStripMenuItem.Click += new System.EventHandler(this.quickFindToolStripMenuItem_Click);
            // 
            // goToToolStripMenuItem
            // 
            this.goToToolStripMenuItem.Name = "goToToolStripMenuItem";
            this.goToToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.G)));
            this.goToToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.goToToolStripMenuItem.Text = "&Go To...";
            this.goToToolStripMenuItem.Click += new System.EventHandler(this.goToToolStripMenuItem_Click);
            // 
            // fileVariablesToolStripMenuItem
            // 
            this.fileVariablesToolStripMenuItem.Name = "fileVariablesToolStripMenuItem";
            this.fileVariablesToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.fileVariablesToolStripMenuItem.Text = "File &Variables...";
            this.fileVariablesToolStripMenuItem.Click += new System.EventHandler(this.fileVariablesToolStripMenuItem_Click);
            // 
            // FontGrowToolStripMenuItem
            // 
            this.FontGrowToolStripMenuItem.Name = "FontGrowToolStripMenuItem";
            this.FontGrowToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)(((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Shift) 
            | System.Windows.Forms.Keys.OemPeriod)));
            this.FontGrowToolStripMenuItem.ShowShortcutKeys = false;
            this.FontGrowToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.FontGrowToolStripMenuItem.Text = "Font G&row";
            this.FontGrowToolStripMenuItem.Click += new System.EventHandler(this.FontGrowToolStripMenuItem_Click);
            // 
            // FontShrinkToolStripMenuItem
            // 
            this.FontShrinkToolStripMenuItem.Name = "FontShrinkToolStripMenuItem";
            this.FontShrinkToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)(((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Shift) 
            | System.Windows.Forms.Keys.Oemcomma)));
            this.FontShrinkToolStripMenuItem.ShowShortcutKeys = false;
            this.FontShrinkToolStripMenuItem.Size = new System.Drawing.Size(230, 30);
            this.FontShrinkToolStripMenuItem.Text = "Font Shrin&k";
            this.FontShrinkToolStripMenuItem.Click += new System.EventHandler(this.FontShrinkToolStripMenuItem_Click);
            // 
            // viewToolStripMenuItem
            // 
            this.viewToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.autoUpdateToolStripMenuItem,
            this.bitstreamToolStripMenuItem,
            this.bitstreamFromClipboardToolStripMenuItem,
            this.ColorMenuItem,
            this.debugInformationToolStripMenuItem,
            this.errorListToolStripMenuItem,
            this.renderToolStripMenuItem,
            this.outputToolStripMenuItem});
            this.viewToolStripMenuItem.Name = "viewToolStripMenuItem";
            this.viewToolStripMenuItem.Size = new System.Drawing.Size(61, 29);
            this.viewToolStripMenuItem.Text = "&View";
            // 
            // autoUpdateToolStripMenuItem
            // 
            this.autoUpdateToolStripMenuItem.Name = "autoUpdateToolStripMenuItem";
            this.autoUpdateToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.autoUpdateToolStripMenuItem.Text = "&Auto-Update";
            this.autoUpdateToolStripMenuItem.Click += new System.EventHandler(this.autoUpdateToolStripMenuItem_Click);
            // 
            // bitstreamToolStripMenuItem
            // 
            this.bitstreamToolStripMenuItem.Name = "bitstreamToolStripMenuItem";
            this.bitstreamToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.bitstreamToolStripMenuItem.Text = "&Bitstream";
            this.bitstreamToolStripMenuItem.Click += new System.EventHandler(this.bitstreamToolStripMenuItem_Click);
            // 
            // bitstreamFromClipboardToolStripMenuItem
            // 
            this.bitstreamFromClipboardToolStripMenuItem.Name = "bitstreamFromClipboardToolStripMenuItem";
            this.bitstreamFromClipboardToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.bitstreamFromClipboardToolStripMenuItem.Text = "Bitstream from clipboard";
            this.bitstreamFromClipboardToolStripMenuItem.Click += new System.EventHandler(this.bitstreamFromClipboardToolStripMenuItem_Click);
            // 
            // ColorMenuItem
            // 
            this.ColorMenuItem.Name = "ColorMenuItem";
            this.ColorMenuItem.Size = new System.Drawing.Size(294, 30);
            this.ColorMenuItem.Text = "&Color";
            this.ColorMenuItem.Click += new System.EventHandler(this.colorToolStripMenuItem_Click);
            // 
            // debugInformationToolStripMenuItem
            // 
            this.debugInformationToolStripMenuItem.Name = "debugInformationToolStripMenuItem";
            this.debugInformationToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.debugInformationToolStripMenuItem.Text = "&Debug Information";
            this.debugInformationToolStripMenuItem.Click += new System.EventHandler(this.debugInformationToolStripMenuItem_Click);
            // 
            // errorListToolStripMenuItem
            // 
            this.errorListToolStripMenuItem.Name = "errorListToolStripMenuItem";
            this.errorListToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.errorListToolStripMenuItem.Text = "Error L&ist";
            this.errorListToolStripMenuItem.Click += new System.EventHandler(this.errorListToolStripMenuItem_Click);
            // 
            // renderToolStripMenuItem
            // 
            this.renderToolStripMenuItem.Name = "renderToolStripMenuItem";
            this.renderToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.renderToolStripMenuItem.Text = "&Render";
            this.renderToolStripMenuItem.Click += new System.EventHandler(this.renderToolStripMenuItem_Click);
            // 
            // outputToolStripMenuItem
            // 
            this.outputToolStripMenuItem.Name = "outputToolStripMenuItem";
            this.outputToolStripMenuItem.Size = new System.Drawing.Size(294, 30);
            this.outputToolStripMenuItem.Text = "&Output";
            this.outputToolStripMenuItem.Click += new System.EventHandler(this.outputToolStripMenuItem_Click);
            // 
            // buildToolStripMenuItem
            // 
            this.buildToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.compileToolStripMenuItem,
            this.exportCompiledObjectToolStripMenuItem});
            this.buildToolStripMenuItem.Name = "buildToolStripMenuItem";
            this.buildToolStripMenuItem.Size = new System.Drawing.Size(63, 29);
            this.buildToolStripMenuItem.Text = "&Build";
            // 
            // compileToolStripMenuItem
            // 
            this.compileToolStripMenuItem.Name = "compileToolStripMenuItem";
            this.compileToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.F7)));
            this.compileToolStripMenuItem.Size = new System.Drawing.Size(286, 30);
            this.compileToolStripMenuItem.Text = "Co&mpile";
            this.compileToolStripMenuItem.Click += new System.EventHandler(this.compileToolStripMenuItem_Click);
            // 
            // exportCompiledObjectToolStripMenuItem
            // 
            this.exportCompiledObjectToolStripMenuItem.Name = "exportCompiledObjectToolStripMenuItem";
            this.exportCompiledObjectToolStripMenuItem.Size = new System.Drawing.Size(286, 30);
            this.exportCompiledObjectToolStripMenuItem.Text = "&Export Compiled Object";
            this.exportCompiledObjectToolStripMenuItem.Click += new System.EventHandler(this.exportCompiledObjectToolStripMenuItem_Click);
            // 
            // toolsToolStripMenuItem
            // 
            this.toolsToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.optionsToolStripMenuItem,
            this.rewriterToolStripMenuItem,
            this.rewriteNobodyToolStripMenuItem});
            this.toolsToolStripMenuItem.Name = "toolsToolStripMenuItem";
            this.toolsToolStripMenuItem.Size = new System.Drawing.Size(65, 29);
            this.toolsToolStripMenuItem.Text = "&Tools";
            // 
            // optionsToolStripMenuItem
            // 
            this.optionsToolStripMenuItem.Name = "optionsToolStripMenuItem";
            this.optionsToolStripMenuItem.Size = new System.Drawing.Size(219, 30);
            this.optionsToolStripMenuItem.Text = "&Options...";
            this.optionsToolStripMenuItem.Click += new System.EventHandler(this.optionsToolStripMenuItem_Click);
            // 
            // rewriterToolStripMenuItem
            // 
            this.rewriterToolStripMenuItem.Name = "rewriterToolStripMenuItem";
            this.rewriterToolStripMenuItem.Size = new System.Drawing.Size(219, 30);
            this.rewriterToolStripMenuItem.Text = "Rewriter";
            this.rewriterToolStripMenuItem.Click += new System.EventHandler(this.rewriterToolStripMenuItem_Click);
            // 
            // rewriteNobodyToolStripMenuItem
            // 
            this.rewriteNobodyToolStripMenuItem.Name = "rewriteNobodyToolStripMenuItem";
            this.rewriteNobodyToolStripMenuItem.Size = new System.Drawing.Size(219, 30);
            this.rewriteNobodyToolStripMenuItem.Text = "RewriteNobody";
            this.rewriteNobodyToolStripMenuItem.Click += new System.EventHandler(this.rewriteNobodyToolStripMenuItem_Click);
            // 
            // helpToolStripMenuItem
            // 
            this.helpToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.aboutToolStripMenuItem});
            this.helpToolStripMenuItem.Name = "helpToolStripMenuItem";
            this.helpToolStripMenuItem.Size = new System.Drawing.Size(61, 29);
            this.helpToolStripMenuItem.Text = "&Help";
            // 
            // aboutToolStripMenuItem
            // 
            this.aboutToolStripMenuItem.Name = "aboutToolStripMenuItem";
            this.aboutToolStripMenuItem.Size = new System.Drawing.Size(158, 30);
            this.aboutToolStripMenuItem.Text = "&About...";
            this.aboutToolStripMenuItem.Click += new System.EventHandler(this.aboutToolStripMenuItem_Click);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.CodeBox);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.AnalysisTabControl);
            this.splitContainer1.Size = new System.Drawing.Size(902, 259);
            this.splitContainer1.SplitterDistance = 324;
            this.splitContainer1.SplitterWidth = 3;
            this.splitContainer1.TabIndex = 2;
            // 
            // CodeBox
            // 
            this.CodeBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.CodeBox.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.CodeBox.Location = new System.Drawing.Point(0, 0);
            this.CodeBox.Name = "CodeBox";
            this.CodeBox.Size = new System.Drawing.Size(324, 259);
            this.CodeBox.TabIndex = 0;
            this.CodeBox.Text = "";
            this.CodeBox.WordWrap = false;
            this.CodeBox.SelectionChanged += new System.EventHandler(this.CodeBox_SelectionChanged);
            this.CodeBox.TextChanged += new System.EventHandler(this.CodeBox_TextChanged);
            this.CodeBox.HelpRequested += new System.Windows.Forms.HelpEventHandler(this.CodeBox_HelpRequested);
            // 
            // AnalysisTabControl
            // 
            this.AnalysisTabControl.Controls.Add(this.CompilationTabPage);
            this.AnalysisTabControl.Controls.Add(this.DisassemblyTabPage);
            this.AnalysisTabControl.Controls.Add(this.ASTTabPage);
            this.AnalysisTabControl.Controls.Add(this.OptimizerTabPage);
            this.AnalysisTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.AnalysisTabControl.Location = new System.Drawing.Point(0, 0);
            this.AnalysisTabControl.Name = "AnalysisTabControl";
            this.AnalysisTabControl.SelectedIndex = 0;
            this.AnalysisTabControl.Size = new System.Drawing.Size(575, 259);
            this.AnalysisTabControl.TabIndex = 0;
            this.AnalysisTabControl.Selecting += new System.Windows.Forms.TabControlCancelEventHandler(this.AnalysisTabControl_Selecting);
            // 
            // CompilationTabPage
            // 
            this.CompilationTabPage.Controls.Add(this.btnCompile);
            this.CompilationTabPage.Controls.Add(this.tbOptions);
            this.CompilationTabPage.Controls.Add(this.label6);
            this.CompilationTabPage.Controls.Add(this.cbProfile);
            this.CompilationTabPage.Controls.Add(this.label5);
            this.CompilationTabPage.Controls.Add(this.tbEntry);
            this.CompilationTabPage.Controls.Add(this.label4);
            this.CompilationTabPage.Location = new System.Drawing.Point(4, 29);
            this.CompilationTabPage.Name = "CompilationTabPage";
            this.CompilationTabPage.Size = new System.Drawing.Size(567, 226);
            this.CompilationTabPage.TabIndex = 3;
            this.CompilationTabPage.Text = "Compilation";
            this.CompilationTabPage.UseVisualStyleBackColor = true;
            // 
            // btnCompile
            // 
            this.btnCompile.AutoSize = true;
            this.btnCompile.Location = new System.Drawing.Point(202, 20);
            this.btnCompile.Name = "btnCompile";
            this.btnCompile.Size = new System.Drawing.Size(213, 55);
            this.btnCompile.TabIndex = 2;
            this.btnCompile.Text = "Compile (Ctrl+F7)";
            this.btnCompile.UseVisualStyleBackColor = true;
            this.btnCompile.Click += new System.EventHandler(this.compileToolStripMenuItem_Click);
            // 
            // tbOptions
            // 
            this.tbOptions.Location = new System.Drawing.Point(6, 120);
            this.tbOptions.Name = "tbOptions";
            this.tbOptions.Size = new System.Drawing.Size(427, 26);
            this.tbOptions.TabIndex = 3;
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(3, 97);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(68, 20);
            this.label6.TabIndex = 7;
            this.label6.Text = "Options:";
            // 
            // cbProfile
            // 
            this.cbProfile.FormattingEnabled = true;
            this.cbProfile.Items.AddRange(new object[] {
            "ps_6_0",
            "ps_6_1",
            "ps_6_2",
            "ps_6_3",
            "vs_6_0",
            "vs_6_1",
            "vs_6_2",
            "vs_6_3",
            "cs_6_0",
            "cs_6_1",
            "cs_6_2",
            "cs_6_3",
            "gs_6_0",
            "gs_6_1",
            "gs_6_2",
            "gs_6_3",
            "hs_6_0",
            "hs_6_1",
            "hs_6_2",
            "hs_6_3",
            "ds_6_0",
            "ds_6_1",
            "ds_6_2",
            "ds_6_3",
            "lib_6_1",
            "lib_6_2",
            "lib_6_3"});
            this.cbProfile.Location = new System.Drawing.Point(6, 69);
            this.cbProfile.Name = "cbProfile";
            this.cbProfile.Size = new System.Drawing.Size(151, 28);
            this.cbProfile.TabIndex = 0;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(3, 48);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(112, 20);
            this.label5.TabIndex = 6;
            this.label5.Text = "Shader Model:";
            // 
            // tbEntry
            // 
            this.tbEntry.Location = new System.Drawing.Point(6, 20);
            this.tbEntry.Name = "tbEntry";
            this.tbEntry.Size = new System.Drawing.Size(160, 26);
            this.tbEntry.TabIndex = 4;
            this.tbEntry.Text = "main";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(3, 0);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(50, 20);
            this.label4.TabIndex = 5;
            this.label4.Text = "Entry:";
            // 
            // DisassemblyTabPage
            // 
            this.DisassemblyTabPage.Controls.Add(this.DisassemblyTextBox);
            this.DisassemblyTabPage.Location = new System.Drawing.Point(4, 29);
            this.DisassemblyTabPage.Name = "DisassemblyTabPage";
            this.DisassemblyTabPage.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.DisassemblyTabPage.Size = new System.Drawing.Size(566, 218);
            this.DisassemblyTabPage.TabIndex = 0;
            this.DisassemblyTabPage.Text = "Disassembly";
            this.DisassemblyTabPage.UseVisualStyleBackColor = true;
            // 
            // DisassemblyTextBox
            // 
            this.DisassemblyTextBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.DisassemblyTextBox.Location = new System.Drawing.Point(3, 3);
            this.DisassemblyTextBox.Name = "DisassemblyTextBox";
            this.DisassemblyTextBox.ReadOnly = true;
            this.DisassemblyTextBox.Size = new System.Drawing.Size(560, 212);
            this.DisassemblyTextBox.TabIndex = 0;
            this.DisassemblyTextBox.Text = "";
            this.DisassemblyTextBox.WordWrap = false;
            this.DisassemblyTextBox.SelectionChanged += new System.EventHandler(this.DisassemblyTextBox_SelectionChanged);
            // 
            // ASTTabPage
            // 
            this.ASTTabPage.Controls.Add(this.ASTDumpBox);
            this.ASTTabPage.Location = new System.Drawing.Point(4, 29);
            this.ASTTabPage.Name = "ASTTabPage";
            this.ASTTabPage.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.ASTTabPage.Size = new System.Drawing.Size(566, 218);
            this.ASTTabPage.TabIndex = 1;
            this.ASTTabPage.Text = "AST";
            this.ASTTabPage.UseVisualStyleBackColor = true;
            // 
            // ASTDumpBox
            // 
            this.ASTDumpBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.ASTDumpBox.Location = new System.Drawing.Point(3, 3);
            this.ASTDumpBox.Name = "ASTDumpBox";
            this.ASTDumpBox.ReadOnly = true;
            this.ASTDumpBox.Size = new System.Drawing.Size(560, 212);
            this.ASTDumpBox.TabIndex = 0;
            this.ASTDumpBox.Text = "";
            // 
            // OptimizerTabPage
            // 
            this.OptimizerTabPage.Controls.Add(this.InteractiveEditorButton);
            this.OptimizerTabPage.Controls.Add(this.ResetDefaultPassesButton);
            this.OptimizerTabPage.Controls.Add(this.AnalyzeCheckBox);
            this.OptimizerTabPage.Controls.Add(this.AddPrintModuleButton);
            this.OptimizerTabPage.Controls.Add(this.RunPassesButton);
            this.OptimizerTabPage.Controls.Add(this.SelectPassDownButton);
            this.OptimizerTabPage.Controls.Add(this.SelectPassUpButton);
            this.OptimizerTabPage.Controls.Add(this.SelectedPassesBox);
            this.OptimizerTabPage.Controls.Add(this.label2);
            this.OptimizerTabPage.Controls.Add(this.label1);
            this.OptimizerTabPage.Controls.Add(this.AvailablePassesBox);
            this.OptimizerTabPage.Location = new System.Drawing.Point(4, 29);
            this.OptimizerTabPage.Name = "OptimizerTabPage";
            this.OptimizerTabPage.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.OptimizerTabPage.Size = new System.Drawing.Size(566, 218);
            this.OptimizerTabPage.TabIndex = 2;
            this.OptimizerTabPage.Text = "Optimizer";
            this.OptimizerTabPage.UseVisualStyleBackColor = true;
            // 
            // InteractiveEditorButton
            // 
            this.InteractiveEditorButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.InteractiveEditorButton.Location = new System.Drawing.Point(330, 166);
            this.InteractiveEditorButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.InteractiveEditorButton.Name = "InteractiveEditorButton";
            this.InteractiveEditorButton.Size = new System.Drawing.Size(168, 28);
            this.InteractiveEditorButton.TabIndex = 11;
            this.InteractiveEditorButton.Text = "Interactive Editor...";
            this.InteractiveEditorButton.UseVisualStyleBackColor = true;
            this.InteractiveEditorButton.Click += new System.EventHandler(this.InteractiveEditorButton_Click);
            // 
            // ResetDefaultPassesButton
            // 
            this.ResetDefaultPassesButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.ResetDefaultPassesButton.Location = new System.Drawing.Point(330, 100);
            this.ResetDefaultPassesButton.Name = "ResetDefaultPassesButton";
            this.ResetDefaultPassesButton.Size = new System.Drawing.Size(168, 29);
            this.ResetDefaultPassesButton.TabIndex = 9;
            this.ResetDefaultPassesButton.Text = "Reset Default Passes";
            this.ResetDefaultPassesButton.UseVisualStyleBackColor = true;
            this.ResetDefaultPassesButton.Click += new System.EventHandler(this.ResetDefaultPassesButton_Click);
            // 
            // AnalyzeCheckBox
            // 
            this.AnalyzeCheckBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.AnalyzeCheckBox.AutoSize = true;
            this.AnalyzeCheckBox.Location = new System.Drawing.Point(6, 59);
            this.AnalyzeCheckBox.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.AnalyzeCheckBox.Name = "AnalyzeCheckBox";
            this.AnalyzeCheckBox.Size = new System.Drawing.Size(146, 24);
            this.AnalyzeCheckBox.TabIndex = 8;
            this.AnalyzeCheckBox.Text = "Analyze passes";
            this.AnalyzeCheckBox.UseVisualStyleBackColor = true;
            // 
            // AddPrintModuleButton
            // 
            this.AddPrintModuleButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.AddPrintModuleButton.Location = new System.Drawing.Point(4, 89);
            this.AddPrintModuleButton.Name = "AddPrintModuleButton";
            this.AddPrintModuleButton.Size = new System.Drawing.Size(168, 29);
            this.AddPrintModuleButton.TabIndex = 7;
            this.AddPrintModuleButton.Text = "Add Print Module";
            this.AddPrintModuleButton.UseVisualStyleBackColor = true;
            this.AddPrintModuleButton.Click += new System.EventHandler(this.AddPrintModuleButton_Click);
            // 
            // RunPassesButton
            // 
            this.RunPassesButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.RunPassesButton.Location = new System.Drawing.Point(330, 132);
            this.RunPassesButton.Name = "RunPassesButton";
            this.RunPassesButton.Size = new System.Drawing.Size(168, 29);
            this.RunPassesButton.TabIndex = 6;
            this.RunPassesButton.Text = "Run Passes";
            this.RunPassesButton.UseVisualStyleBackColor = true;
            this.RunPassesButton.Click += new System.EventHandler(this.RunPassesButton_Click);
            // 
            // SelectPassDownButton
            // 
            this.SelectPassDownButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.SelectPassDownButton.Location = new System.Drawing.Point(416, 63);
            this.SelectPassDownButton.Name = "SelectPassDownButton";
            this.SelectPassDownButton.Size = new System.Drawing.Size(86, 29);
            this.SelectPassDownButton.TabIndex = 5;
            this.SelectPassDownButton.Text = "Swap Down";
            this.SelectPassDownButton.UseVisualStyleBackColor = true;
            this.SelectPassDownButton.Click += new System.EventHandler(this.SelectPassDownButton_Click);
            // 
            // SelectPassUpButton
            // 
            this.SelectPassUpButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.SelectPassUpButton.Location = new System.Drawing.Point(330, 63);
            this.SelectPassUpButton.Name = "SelectPassUpButton";
            this.SelectPassUpButton.Size = new System.Drawing.Size(81, 29);
            this.SelectPassUpButton.TabIndex = 4;
            this.SelectPassUpButton.Text = "Swap Up";
            this.SelectPassUpButton.UseVisualStyleBackColor = true;
            this.SelectPassUpButton.Click += new System.EventHandler(this.SelectPassUpButton_Click);
            // 
            // SelectedPassesBox
            // 
            this.SelectedPassesBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.SelectedPassesBox.ContextMenuStrip = this.PassesContextMenu;
            this.SelectedPassesBox.FormattingEnabled = true;
            this.SelectedPassesBox.IntegralHeight = false;
            this.SelectedPassesBox.ItemHeight = 20;
            this.SelectedPassesBox.Location = new System.Drawing.Point(330, 37);
            this.SelectedPassesBox.Name = "SelectedPassesBox";
            this.SelectedPassesBox.Size = new System.Drawing.Size(236, 21);
            this.SelectedPassesBox.TabIndex = 3;
            this.SelectedPassesBox.DoubleClick += new System.EventHandler(this.SelectedPassesBox_DoubleClick);
            this.SelectedPassesBox.KeyUp += new System.Windows.Forms.KeyEventHandler(this.SelectedPassesBox_KeyUp);
            // 
            // PassesContextMenu
            // 
            this.PassesContextMenu.ImageScalingSize = new System.Drawing.Size(32, 32);
            this.PassesContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.PassPropertiesMenuItem,
            this.toolStripMenuItem5,
            this.copyToolStripMenuItem1,
            this.copyAllToolStripMenuItem,
            this.PastePassesMenuItem,
            this.DeleteAllPassesMenuItem});
            this.PassesContextMenu.Name = "PassesContextMenu";
            this.PassesContextMenu.Size = new System.Drawing.Size(177, 160);
            // 
            // PassPropertiesMenuItem
            // 
            this.PassPropertiesMenuItem.Name = "PassPropertiesMenuItem";
            this.PassPropertiesMenuItem.Size = new System.Drawing.Size(176, 30);
            this.PassPropertiesMenuItem.Text = "P&roperties...";
            this.PassPropertiesMenuItem.Click += new System.EventHandler(this.PassPropertiesMenuItem_Click);
            // 
            // toolStripMenuItem5
            // 
            this.toolStripMenuItem5.Name = "toolStripMenuItem5";
            this.toolStripMenuItem5.Size = new System.Drawing.Size(173, 6);
            // 
            // copyToolStripMenuItem1
            // 
            this.copyToolStripMenuItem1.Name = "copyToolStripMenuItem1";
            this.copyToolStripMenuItem1.Size = new System.Drawing.Size(176, 30);
            this.copyToolStripMenuItem1.Text = "&Copy";
            this.copyToolStripMenuItem1.Click += new System.EventHandler(this.copyToolStripMenuItem_Click);
            // 
            // copyAllToolStripMenuItem
            // 
            this.copyAllToolStripMenuItem.Name = "copyAllToolStripMenuItem";
            this.copyAllToolStripMenuItem.Size = new System.Drawing.Size(176, 30);
            this.copyAllToolStripMenuItem.Text = "Copy &All";
            this.copyAllToolStripMenuItem.Click += new System.EventHandler(this.copyAllToolStripMenuItem_Click);
            // 
            // PastePassesMenuItem
            // 
            this.PastePassesMenuItem.Name = "PastePassesMenuItem";
            this.PastePassesMenuItem.Size = new System.Drawing.Size(176, 30);
            this.PastePassesMenuItem.Text = "&Paste";
            this.PastePassesMenuItem.Click += new System.EventHandler(this.PastePassesMenuItem_Click);
            // 
            // DeleteAllPassesMenuItem
            // 
            this.DeleteAllPassesMenuItem.Name = "DeleteAllPassesMenuItem";
            this.DeleteAllPassesMenuItem.Size = new System.Drawing.Size(176, 30);
            this.DeleteAllPassesMenuItem.Text = "Delete All";
            this.DeleteAllPassesMenuItem.Click += new System.EventHandler(this.DeleteAllPassesMenuItem_Click);
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(326, 9);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(68, 20);
            this.label2.TabIndex = 2;
            this.label2.Text = "&Pipeline:";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(4, 9);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(278, 20);
            this.label1.TabIndex = 1;
            this.label1.Text = "&Available Passes (double-click to add):";
            // 
            // AvailablePassesBox
            // 
            this.AvailablePassesBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.AvailablePassesBox.FormattingEnabled = true;
            this.AvailablePassesBox.IntegralHeight = false;
            this.AvailablePassesBox.ItemHeight = 20;
            this.AvailablePassesBox.Location = new System.Drawing.Point(6, 37);
            this.AvailablePassesBox.Name = "AvailablePassesBox";
            this.AvailablePassesBox.SelectionMode = System.Windows.Forms.SelectionMode.MultiExtended;
            this.AvailablePassesBox.Size = new System.Drawing.Size(316, 21);
            this.AvailablePassesBox.TabIndex = 0;
            this.AvailablePassesBox.DoubleClick += new System.EventHandler(this.AvailablePassesBox_DoubleClick);
            // 
            // TopSplitContainer
            // 
            this.TopSplitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TopSplitContainer.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
            this.TopSplitContainer.Location = new System.Drawing.Point(0, 33);
            this.TopSplitContainer.Name = "TopSplitContainer";
            this.TopSplitContainer.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // TopSplitContainer.Panel1
            // 
            this.TopSplitContainer.Panel1.Controls.Add(this.splitContainer1);
            // 
            // TopSplitContainer.Panel2
            // 
            this.TopSplitContainer.Panel2.Controls.Add(this.OutputTabControl);
            this.TopSplitContainer.Panel2Collapsed = true;
            this.TopSplitContainer.Panel2MinSize = 75;
            this.TopSplitContainer.Size = new System.Drawing.Size(902, 259);
            this.TopSplitContainer.SplitterDistance = 25;
            this.TopSplitContainer.SplitterWidth = 3;
            this.TopSplitContainer.TabIndex = 3;
            // 
            // OutputTabControl
            // 
            this.OutputTabControl.Controls.Add(this.RenderLogTabPage);
            this.OutputTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.OutputTabControl.Location = new System.Drawing.Point(0, 0);
            this.OutputTabControl.Name = "OutputTabControl";
            this.OutputTabControl.SelectedIndex = 0;
            this.OutputTabControl.Size = new System.Drawing.Size(225, 71);
            this.OutputTabControl.TabIndex = 0;
            // 
            // RenderLogTabPage
            // 
            this.RenderLogTabPage.Controls.Add(this.RenderLogBox);
            this.RenderLogTabPage.Location = new System.Drawing.Point(6, 34);
            this.RenderLogTabPage.Name = "RenderLogTabPage";
            this.RenderLogTabPage.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.RenderLogTabPage.Size = new System.Drawing.Size(213, 31);
            this.RenderLogTabPage.TabIndex = 0;
            this.RenderLogTabPage.Text = "Render Log";
            this.RenderLogTabPage.UseVisualStyleBackColor = true;
            // 
            // RenderLogBox
            // 
            this.RenderLogBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.RenderLogBox.Location = new System.Drawing.Point(3, 3);
            this.RenderLogBox.Multiline = true;
            this.RenderLogBox.Name = "RenderLogBox";
            this.RenderLogBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.RenderLogBox.Size = new System.Drawing.Size(207, 25);
            this.RenderLogBox.TabIndex = 0;
            this.RenderLogBox.WordWrap = false;
            // 
            // RewriterOutputTextBox
            // 
            this.RewriterOutputTextBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.RewriterOutputTextBox.Location = new System.Drawing.Point(4, 4);
            this.RewriterOutputTextBox.Margin = new System.Windows.Forms.Padding(4);
            this.RewriterOutputTextBox.Name = "RewriterOutputTextBox";
            this.RewriterOutputTextBox.ReadOnly = true;
            this.RewriterOutputTextBox.Size = new System.Drawing.Size(970, 937);
            this.RewriterOutputTextBox.TabIndex = 1;
            this.RewriterOutputTextBox.Text = "";
            this.RewriterOutputTextBox.WordWrap = false;
            // 
            // EditorForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(902, 322);
            this.Controls.Add(this.TopSplitContainer);
            this.Controls.Add(this.TheStatusStrip);
            this.Controls.Add(this.TheMenuStrip);
            this.MainMenuStrip = this.TheMenuStrip;
            this.Name = "EditorForm";
            this.Text = "DirectX Compiler Editor";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.EditorForm_FormClosing);
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.EditorForm_FormClosed);
            this.Load += new System.EventHandler(this.EditorForm_Load);
            this.Shown += new System.EventHandler(this.EditorForm_Shown);
            this.TheStatusStrip.ResumeLayout(false);
            this.TheStatusStrip.PerformLayout();
            this.TheMenuStrip.ResumeLayout(false);
            this.TheMenuStrip.PerformLayout();
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.AnalysisTabControl.ResumeLayout(false);
            this.CompilationTabPage.ResumeLayout(false);
            this.CompilationTabPage.PerformLayout();
            this.DisassemblyTabPage.ResumeLayout(false);
            this.ASTTabPage.ResumeLayout(false);
            this.OptimizerTabPage.ResumeLayout(false);
            this.OptimizerTabPage.PerformLayout();
            this.PassesContextMenu.ResumeLayout(false);
            this.TopSplitContainer.Panel1.ResumeLayout(false);
            this.TopSplitContainer.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.TopSplitContainer)).EndInit();
            this.TopSplitContainer.ResumeLayout(false);
            this.OutputTabControl.ResumeLayout(false);
            this.RenderLogTabPage.ResumeLayout(false);
            this.RenderLogTabPage.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.StatusStrip TheStatusStrip;
        private System.Windows.Forms.ToolStripStatusLabel TheStatusStripLabel;
        private System.Windows.Forms.MenuStrip TheMenuStrip;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.RichTextBox CodeBox;
        private System.Windows.Forms.ToolStripMenuItem editToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem viewToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem NewToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem undoToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem1;
        private System.Windows.Forms.ToolStripMenuItem cutToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem copyToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem pasteToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem deleteToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem2;
        private System.Windows.Forms.ToolStripMenuItem selectAllToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem3;
        private System.Windows.Forms.ToolStripMenuItem goToToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem bitstreamToolStripMenuItem;
        private System.Windows.Forms.ToolTip TheToolTip;
        private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem saveToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem saveAsToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem buildToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem compileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem autoUpdateToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exportCompiledObjectToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem fileVariablesToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem recentFilesToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem4;
        private System.Windows.Forms.ToolStripMenuItem findAndReplaceToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem quickFindToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem errorListToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem helpToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem aboutToolStripMenuItem;
        private System.Windows.Forms.ContextMenuStrip PassesContextMenu;
        private System.Windows.Forms.ToolStripMenuItem PassPropertiesMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem5;
        private System.Windows.Forms.ToolStripMenuItem copyToolStripMenuItem1;
        private System.Windows.Forms.ToolStripMenuItem copyAllToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem renderToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem outputToolStripMenuItem;
        private System.Windows.Forms.SplitContainer TopSplitContainer;
        private System.Windows.Forms.TabControl OutputTabControl;
        private System.Windows.Forms.TabPage RenderLogTabPage;
        private System.Windows.Forms.TextBox RenderLogBox;
        private System.Windows.Forms.ToolStripMenuItem FontGrowToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem FontShrinkToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem toolsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem optionsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem ColorMenuItem;
        private System.Windows.Forms.ToolStripMenuItem rewriterToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem rewriteNobodyToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem debugInformationToolStripMenuItem;
        private System.Windows.Forms.TabControl AnalysisTabControl;
        private System.Windows.Forms.TabPage DisassemblyTabPage;
        private System.Windows.Forms.RichTextBox DisassemblyTextBox;
        private System.Windows.Forms.TabPage ASTTabPage;
        private System.Windows.Forms.RichTextBox ASTDumpBox;
        private System.Windows.Forms.TabPage OptimizerTabPage;
        private System.Windows.Forms.Button ResetDefaultPassesButton;
        private System.Windows.Forms.CheckBox AnalyzeCheckBox;
        private System.Windows.Forms.Button AddPrintModuleButton;
        private System.Windows.Forms.Button RunPassesButton;
        private System.Windows.Forms.Button SelectPassDownButton;
        private System.Windows.Forms.Button SelectPassUpButton;
        private System.Windows.Forms.ListBox SelectedPassesBox;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.ListBox AvailablePassesBox;
        private System.Windows.Forms.RichTextBox RewriterOutputTextBox;
        private System.Windows.Forms.ToolStripMenuItem PastePassesMenuItem;
        private System.Windows.Forms.ToolStripMenuItem DeleteAllPassesMenuItem;
        private System.Windows.Forms.Button InteractiveEditorButton;
        private System.Windows.Forms.ComboBox cbProfile;
        private System.Windows.Forms.Button btnCompile;
        private System.Windows.Forms.TextBox tbOptions;
        private System.Windows.Forms.TextBox tbEntry;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.TabPage CompilationTabPage;
        private System.Windows.Forms.ToolStripMenuItem bitstreamFromClipboardToolStripMenuItem;
    }
}