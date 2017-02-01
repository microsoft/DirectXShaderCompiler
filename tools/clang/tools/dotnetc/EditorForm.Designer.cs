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
            this.viewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.autoUpdateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.bitstreamToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.errorListToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.renderToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.outputToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.buildToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.compileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exportCompiledObjectToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.CodeBox = new System.Windows.Forms.RichTextBox();
            this.AnalysisTabControl = new System.Windows.Forms.TabControl();
            this.DisassemblyTabPage = new System.Windows.Forms.TabPage();
            this.DisassemblyTextBox = new System.Windows.Forms.RichTextBox();
            this.ASTTabPage = new System.Windows.Forms.TabPage();
            this.ASTDumpBox = new System.Windows.Forms.RichTextBox();
            this.OptimizerTabPage = new System.Windows.Forms.TabPage();
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
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.AvailablePassesBox = new System.Windows.Forms.ListBox();
            this.TheToolTip = new System.Windows.Forms.ToolTip(this.components);
            this.TopSplitContainer = new System.Windows.Forms.SplitContainer();
            this.OutputTabControl = new System.Windows.Forms.TabControl();
            this.RenderLogTabPage = new System.Windows.Forms.TabPage();
            this.RenderLogBox = new System.Windows.Forms.TextBox();
            this.TheStatusStrip.SuspendLayout();
            this.TheMenuStrip.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.AnalysisTabControl.SuspendLayout();
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
            this.TheStatusStrip.Location = new System.Drawing.Point(0, 1057);
            this.TheStatusStrip.Name = "TheStatusStrip";
            this.TheStatusStrip.Padding = new System.Windows.Forms.Padding(2, 0, 18, 0);
            this.TheStatusStrip.Size = new System.Drawing.Size(1568, 22);
            this.TheStatusStrip.TabIndex = 0;
            this.TheStatusStrip.Text = "statusStrip1";
            // 
            // TheStatusStripLabel
            // 
            this.TheStatusStripLabel.Name = "TheStatusStripLabel";
            this.TheStatusStripLabel.Size = new System.Drawing.Size(0, 17);
            // 
            // TheMenuStrip
            // 
            this.TheMenuStrip.ImageScalingSize = new System.Drawing.Size(24, 24);
            this.TheMenuStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.editToolStripMenuItem,
            this.viewToolStripMenuItem,
            this.buildToolStripMenuItem,
            this.helpToolStripMenuItem});
            this.TheMenuStrip.Location = new System.Drawing.Point(0, 0);
            this.TheMenuStrip.Name = "TheMenuStrip";
            this.TheMenuStrip.Padding = new System.Windows.Forms.Padding(8, 2, 0, 2);
            this.TheMenuStrip.Size = new System.Drawing.Size(1568, 40);
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
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(64, 36);
            this.fileToolStripMenuItem.Text = "&File";
            this.fileToolStripMenuItem.DropDownOpening += new System.EventHandler(this.fileToolStripMenuItem_DropDownOpening);
            // 
            // NewToolStripMenuItem
            // 
            this.NewToolStripMenuItem.Name = "NewToolStripMenuItem";
            this.NewToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.N)));
            this.NewToolStripMenuItem.Size = new System.Drawing.Size(275, 38);
            this.NewToolStripMenuItem.Text = "&New";
            this.NewToolStripMenuItem.Click += new System.EventHandler(this.NewToolStripMenuItem_Click);
            // 
            // openToolStripMenuItem
            // 
            this.openToolStripMenuItem.Name = "openToolStripMenuItem";
            this.openToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.O)));
            this.openToolStripMenuItem.Size = new System.Drawing.Size(275, 38);
            this.openToolStripMenuItem.Text = "&Open...";
            this.openToolStripMenuItem.Click += new System.EventHandler(this.openToolStripMenuItem_Click);
            // 
            // saveToolStripMenuItem
            // 
            this.saveToolStripMenuItem.Name = "saveToolStripMenuItem";
            this.saveToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.S)));
            this.saveToolStripMenuItem.Size = new System.Drawing.Size(275, 38);
            this.saveToolStripMenuItem.Text = "&Save";
            this.saveToolStripMenuItem.Click += new System.EventHandler(this.saveToolStripMenuItem_Click);
            // 
            // saveAsToolStripMenuItem
            // 
            this.saveAsToolStripMenuItem.Name = "saveAsToolStripMenuItem";
            this.saveAsToolStripMenuItem.Size = new System.Drawing.Size(275, 38);
            this.saveAsToolStripMenuItem.Text = "Save &As...";
            this.saveAsToolStripMenuItem.Click += new System.EventHandler(this.saveAsToolStripMenuItem_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(272, 6);
            // 
            // recentFilesToolStripMenuItem
            // 
            this.recentFilesToolStripMenuItem.Name = "recentFilesToolStripMenuItem";
            this.recentFilesToolStripMenuItem.Size = new System.Drawing.Size(275, 38);
            this.recentFilesToolStripMenuItem.Text = "Recent &Files";
            // 
            // toolStripMenuItem4
            // 
            this.toolStripMenuItem4.Name = "toolStripMenuItem4";
            this.toolStripMenuItem4.Size = new System.Drawing.Size(272, 6);
            // 
            // exitToolStripMenuItem
            // 
            this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
            this.exitToolStripMenuItem.Size = new System.Drawing.Size(275, 38);
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
            this.fileVariablesToolStripMenuItem});
            this.editToolStripMenuItem.Name = "editToolStripMenuItem";
            this.editToolStripMenuItem.Size = new System.Drawing.Size(67, 36);
            this.editToolStripMenuItem.Text = "&Edit";
            // 
            // undoToolStripMenuItem
            // 
            this.undoToolStripMenuItem.Name = "undoToolStripMenuItem";
            this.undoToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Z)));
            this.undoToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.undoToolStripMenuItem.Text = "&Undo";
            this.undoToolStripMenuItem.Click += new System.EventHandler(this.undoToolStripMenuItem_Click);
            // 
            // toolStripMenuItem1
            // 
            this.toolStripMenuItem1.Name = "toolStripMenuItem1";
            this.toolStripMenuItem1.Size = new System.Drawing.Size(294, 6);
            // 
            // cutToolStripMenuItem
            // 
            this.cutToolStripMenuItem.Name = "cutToolStripMenuItem";
            this.cutToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.X)));
            this.cutToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.cutToolStripMenuItem.Text = "Cu&t";
            this.cutToolStripMenuItem.Click += new System.EventHandler(this.cutToolStripMenuItem_Click);
            // 
            // copyToolStripMenuItem
            // 
            this.copyToolStripMenuItem.Name = "copyToolStripMenuItem";
            this.copyToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.C)));
            this.copyToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.copyToolStripMenuItem.Text = "&Copy";
            this.copyToolStripMenuItem.Click += new System.EventHandler(this.copyToolStripMenuItem_Click);
            // 
            // pasteToolStripMenuItem
            // 
            this.pasteToolStripMenuItem.Name = "pasteToolStripMenuItem";
            this.pasteToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.V)));
            this.pasteToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.pasteToolStripMenuItem.Text = "&Paste";
            this.pasteToolStripMenuItem.Click += new System.EventHandler(this.pasteToolStripMenuItem_Click);
            // 
            // deleteToolStripMenuItem
            // 
            this.deleteToolStripMenuItem.Name = "deleteToolStripMenuItem";
            this.deleteToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.deleteToolStripMenuItem.Text = "&Delete";
            this.deleteToolStripMenuItem.Click += new System.EventHandler(this.deleteToolStripMenuItem_Click);
            // 
            // toolStripMenuItem2
            // 
            this.toolStripMenuItem2.Name = "toolStripMenuItem2";
            this.toolStripMenuItem2.Size = new System.Drawing.Size(294, 6);
            // 
            // selectAllToolStripMenuItem
            // 
            this.selectAllToolStripMenuItem.Name = "selectAllToolStripMenuItem";
            this.selectAllToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.A)));
            this.selectAllToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.selectAllToolStripMenuItem.Text = "Select &All";
            this.selectAllToolStripMenuItem.Click += new System.EventHandler(this.selectAllToolStripMenuItem_Click);
            // 
            // toolStripMenuItem3
            // 
            this.toolStripMenuItem3.Name = "toolStripMenuItem3";
            this.toolStripMenuItem3.Size = new System.Drawing.Size(294, 6);
            // 
            // findAndReplaceToolStripMenuItem
            // 
            this.findAndReplaceToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.quickFindToolStripMenuItem});
            this.findAndReplaceToolStripMenuItem.Name = "findAndReplaceToolStripMenuItem";
            this.findAndReplaceToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.findAndReplaceToolStripMenuItem.Text = "&Find and Replace";
            // 
            // quickFindToolStripMenuItem
            // 
            this.quickFindToolStripMenuItem.Name = "quickFindToolStripMenuItem";
            this.quickFindToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.F)));
            this.quickFindToolStripMenuItem.Size = new System.Drawing.Size(309, 38);
            this.quickFindToolStripMenuItem.Text = "Quick &Find";
            this.quickFindToolStripMenuItem.Click += new System.EventHandler(this.quickFindToolStripMenuItem_Click);
            // 
            // goToToolStripMenuItem
            // 
            this.goToToolStripMenuItem.Name = "goToToolStripMenuItem";
            this.goToToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.G)));
            this.goToToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.goToToolStripMenuItem.Text = "&Go To...";
            this.goToToolStripMenuItem.Click += new System.EventHandler(this.goToToolStripMenuItem_Click);
            // 
            // fileVariablesToolStripMenuItem
            // 
            this.fileVariablesToolStripMenuItem.Name = "fileVariablesToolStripMenuItem";
            this.fileVariablesToolStripMenuItem.Size = new System.Drawing.Size(297, 38);
            this.fileVariablesToolStripMenuItem.Text = "File &Variables...";
            this.fileVariablesToolStripMenuItem.Click += new System.EventHandler(this.fileVariablesToolStripMenuItem_Click);
            // 
            // viewToolStripMenuItem
            // 
            this.viewToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.autoUpdateToolStripMenuItem,
            this.bitstreamToolStripMenuItem,
            this.errorListToolStripMenuItem,
            this.renderToolStripMenuItem,
            this.outputToolStripMenuItem});
            this.viewToolStripMenuItem.Name = "viewToolStripMenuItem";
            this.viewToolStripMenuItem.Size = new System.Drawing.Size(78, 36);
            this.viewToolStripMenuItem.Text = "&View";
            // 
            // autoUpdateToolStripMenuItem
            // 
            this.autoUpdateToolStripMenuItem.Name = "autoUpdateToolStripMenuItem";
            this.autoUpdateToolStripMenuItem.Size = new System.Drawing.Size(253, 38);
            this.autoUpdateToolStripMenuItem.Text = "&Auto-Update";
            this.autoUpdateToolStripMenuItem.Click += new System.EventHandler(this.autoUpdateToolStripMenuItem_Click);
            // 
            // bitstreamToolStripMenuItem
            // 
            this.bitstreamToolStripMenuItem.Name = "bitstreamToolStripMenuItem";
            this.bitstreamToolStripMenuItem.Size = new System.Drawing.Size(253, 38);
            this.bitstreamToolStripMenuItem.Text = "&Bitstream";
            this.bitstreamToolStripMenuItem.Click += new System.EventHandler(this.bitstreamToolStripMenuItem_Click);
            // 
            // errorListToolStripMenuItem
            // 
            this.errorListToolStripMenuItem.Name = "errorListToolStripMenuItem";
            this.errorListToolStripMenuItem.Size = new System.Drawing.Size(253, 38);
            this.errorListToolStripMenuItem.Text = "Error L&ist";
            this.errorListToolStripMenuItem.Click += new System.EventHandler(this.errorListToolStripMenuItem_Click);
            // 
            // renderToolStripMenuItem
            // 
            this.renderToolStripMenuItem.Name = "renderToolStripMenuItem";
            this.renderToolStripMenuItem.Size = new System.Drawing.Size(253, 38);
            this.renderToolStripMenuItem.Text = "&Render";
            this.renderToolStripMenuItem.Click += new System.EventHandler(this.renderToolStripMenuItem_Click);
            // 
            // outputToolStripMenuItem
            // 
            this.outputToolStripMenuItem.Name = "outputToolStripMenuItem";
            this.outputToolStripMenuItem.Size = new System.Drawing.Size(253, 38);
            this.outputToolStripMenuItem.Text = "&Output";
            this.outputToolStripMenuItem.Click += new System.EventHandler(this.outputToolStripMenuItem_Click);
            // 
            // buildToolStripMenuItem
            // 
            this.buildToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.compileToolStripMenuItem,
            this.exportCompiledObjectToolStripMenuItem});
            this.buildToolStripMenuItem.Name = "buildToolStripMenuItem";
            this.buildToolStripMenuItem.Size = new System.Drawing.Size(81, 36);
            this.buildToolStripMenuItem.Text = "&Build";
            // 
            // compileToolStripMenuItem
            // 
            this.compileToolStripMenuItem.Name = "compileToolStripMenuItem";
            this.compileToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.F7)));
            this.compileToolStripMenuItem.Size = new System.Drawing.Size(369, 38);
            this.compileToolStripMenuItem.Text = "Co&mpile";
            this.compileToolStripMenuItem.Click += new System.EventHandler(this.compileToolStripMenuItem_Click);
            // 
            // exportCompiledObjectToolStripMenuItem
            // 
            this.exportCompiledObjectToolStripMenuItem.Name = "exportCompiledObjectToolStripMenuItem";
            this.exportCompiledObjectToolStripMenuItem.Size = new System.Drawing.Size(369, 38);
            this.exportCompiledObjectToolStripMenuItem.Text = "&Export Compiled Object";
            this.exportCompiledObjectToolStripMenuItem.Click += new System.EventHandler(this.exportCompiledObjectToolStripMenuItem_Click);
            // 
            // helpToolStripMenuItem
            // 
            this.helpToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.aboutToolStripMenuItem});
            this.helpToolStripMenuItem.Name = "helpToolStripMenuItem";
            this.helpToolStripMenuItem.Size = new System.Drawing.Size(77, 36);
            this.helpToolStripMenuItem.Text = "&Help";
            // 
            // aboutToolStripMenuItem
            // 
            this.aboutToolStripMenuItem.Name = "aboutToolStripMenuItem";
            this.aboutToolStripMenuItem.Size = new System.Drawing.Size(195, 38);
            this.aboutToolStripMenuItem.Text = "&About...";
            this.aboutToolStripMenuItem.Click += new System.EventHandler(this.aboutToolStripMenuItem_Click);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Margin = new System.Windows.Forms.Padding(4);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.CodeBox);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.AnalysisTabControl);
            this.splitContainer1.Size = new System.Drawing.Size(1568, 1017);
            this.splitContainer1.SplitterDistance = 570;
            this.splitContainer1.SplitterWidth = 6;
            this.splitContainer1.TabIndex = 2;
            // 
            // CodeBox
            // 
            this.CodeBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.CodeBox.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.CodeBox.Location = new System.Drawing.Point(0, 0);
            this.CodeBox.Margin = new System.Windows.Forms.Padding(4);
            this.CodeBox.Name = "CodeBox";
            this.CodeBox.Size = new System.Drawing.Size(570, 1017);
            this.CodeBox.TabIndex = 0;
            this.CodeBox.Text = "";
            this.CodeBox.WordWrap = false;
            this.CodeBox.SelectionChanged += new System.EventHandler(this.CodeBox_SelectionChanged);
            this.CodeBox.TextChanged += new System.EventHandler(this.CodeBox_TextChanged);
            // 
            // AnalysisTabControl
            // 
            this.AnalysisTabControl.Controls.Add(this.DisassemblyTabPage);
            this.AnalysisTabControl.Controls.Add(this.ASTTabPage);
            this.AnalysisTabControl.Controls.Add(this.OptimizerTabPage);
            this.AnalysisTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.AnalysisTabControl.Location = new System.Drawing.Point(0, 0);
            this.AnalysisTabControl.Margin = new System.Windows.Forms.Padding(4);
            this.AnalysisTabControl.Name = "AnalysisTabControl";
            this.AnalysisTabControl.SelectedIndex = 0;
            this.AnalysisTabControl.Size = new System.Drawing.Size(992, 1017);
            this.AnalysisTabControl.TabIndex = 0;
            this.AnalysisTabControl.Selecting += new System.Windows.Forms.TabControlCancelEventHandler(this.AnalysisTabControl_Selecting);
            // 
            // DisassemblyTabPage
            // 
            this.DisassemblyTabPage.Controls.Add(this.DisassemblyTextBox);
            this.DisassemblyTabPage.Location = new System.Drawing.Point(8, 39);
            this.DisassemblyTabPage.Margin = new System.Windows.Forms.Padding(4);
            this.DisassemblyTabPage.Name = "DisassemblyTabPage";
            this.DisassemblyTabPage.Padding = new System.Windows.Forms.Padding(4);
            this.DisassemblyTabPage.Size = new System.Drawing.Size(976, 970);
            this.DisassemblyTabPage.TabIndex = 0;
            this.DisassemblyTabPage.Text = "Disassembly";
            this.DisassemblyTabPage.UseVisualStyleBackColor = true;
            // 
            // DisassemblyTextBox
            // 
            this.DisassemblyTextBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.DisassemblyTextBox.Location = new System.Drawing.Point(4, 4);
            this.DisassemblyTextBox.Margin = new System.Windows.Forms.Padding(4);
            this.DisassemblyTextBox.Name = "DisassemblyTextBox";
            this.DisassemblyTextBox.ReadOnly = true;
            this.DisassemblyTextBox.Size = new System.Drawing.Size(968, 962);
            this.DisassemblyTextBox.TabIndex = 0;
            this.DisassemblyTextBox.Text = "";
            this.DisassemblyTextBox.WordWrap = false;
            this.DisassemblyTextBox.SelectionChanged += new System.EventHandler(this.DisassemblyTextBox_SelectionChanged);
            // 
            // ASTTabPage
            // 
            this.ASTTabPage.Controls.Add(this.ASTDumpBox);
            this.ASTTabPage.Location = new System.Drawing.Point(8, 39);
            this.ASTTabPage.Margin = new System.Windows.Forms.Padding(4);
            this.ASTTabPage.Name = "ASTTabPage";
            this.ASTTabPage.Padding = new System.Windows.Forms.Padding(4);
            this.ASTTabPage.Size = new System.Drawing.Size(976, 970);
            this.ASTTabPage.TabIndex = 1;
            this.ASTTabPage.Text = "AST";
            this.ASTTabPage.UseVisualStyleBackColor = true;
            // 
            // ASTDumpBox
            // 
            this.ASTDumpBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.ASTDumpBox.Location = new System.Drawing.Point(4, 4);
            this.ASTDumpBox.Margin = new System.Windows.Forms.Padding(4);
            this.ASTDumpBox.Name = "ASTDumpBox";
            this.ASTDumpBox.ReadOnly = true;
            this.ASTDumpBox.Size = new System.Drawing.Size(968, 962);
            this.ASTDumpBox.TabIndex = 0;
            this.ASTDumpBox.Text = "";
            // 
            // OptimizerTabPage
            // 
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
            this.OptimizerTabPage.Location = new System.Drawing.Point(8, 39);
            this.OptimizerTabPage.Margin = new System.Windows.Forms.Padding(4);
            this.OptimizerTabPage.Name = "OptimizerTabPage";
            this.OptimizerTabPage.Padding = new System.Windows.Forms.Padding(4);
            this.OptimizerTabPage.Size = new System.Drawing.Size(976, 970);
            this.OptimizerTabPage.TabIndex = 2;
            this.OptimizerTabPage.Text = "Optimizer";
            this.OptimizerTabPage.UseVisualStyleBackColor = true;
            // 
            // ResetDefaultPassesButton
            // 
            this.ResetDefaultPassesButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.ResetDefaultPassesButton.Location = new System.Drawing.Point(560, 808);
            this.ResetDefaultPassesButton.Margin = new System.Windows.Forms.Padding(4);
            this.ResetDefaultPassesButton.Name = "ResetDefaultPassesButton";
            this.ResetDefaultPassesButton.Size = new System.Drawing.Size(292, 48);
            this.ResetDefaultPassesButton.TabIndex = 9;
            this.ResetDefaultPassesButton.Text = "Reset Default Passes";
            this.ResetDefaultPassesButton.UseVisualStyleBackColor = true;
            this.ResetDefaultPassesButton.Click += new System.EventHandler(this.ResetDefaultPassesButton_Click);
            // 
            // AnalyzeCheckBox
            // 
            this.AnalyzeCheckBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.AnalyzeCheckBox.AutoSize = true;
            this.AnalyzeCheckBox.Location = new System.Drawing.Point(14, 766);
            this.AnalyzeCheckBox.Margin = new System.Windows.Forms.Padding(6);
            this.AnalyzeCheckBox.Name = "AnalyzeCheckBox";
            this.AnalyzeCheckBox.Size = new System.Drawing.Size(196, 29);
            this.AnalyzeCheckBox.TabIndex = 8;
            this.AnalyzeCheckBox.Text = "Analyze passes";
            this.AnalyzeCheckBox.UseVisualStyleBackColor = true;
            // 
            // AddPrintModuleButton
            // 
            this.AddPrintModuleButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.AddPrintModuleButton.Location = new System.Drawing.Point(14, 808);
            this.AddPrintModuleButton.Margin = new System.Windows.Forms.Padding(4);
            this.AddPrintModuleButton.Name = "AddPrintModuleButton";
            this.AddPrintModuleButton.Size = new System.Drawing.Size(292, 48);
            this.AddPrintModuleButton.TabIndex = 7;
            this.AddPrintModuleButton.Text = "Add Print Module";
            this.AddPrintModuleButton.UseVisualStyleBackColor = true;
            this.AddPrintModuleButton.Click += new System.EventHandler(this.AddPrintModuleButton_Click);
            // 
            // RunPassesButton
            // 
            this.RunPassesButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.RunPassesButton.Location = new System.Drawing.Point(560, 860);
            this.RunPassesButton.Margin = new System.Windows.Forms.Padding(4);
            this.RunPassesButton.Name = "RunPassesButton";
            this.RunPassesButton.Size = new System.Drawing.Size(292, 48);
            this.RunPassesButton.TabIndex = 6;
            this.RunPassesButton.Text = "Run Passes";
            this.RunPassesButton.UseVisualStyleBackColor = true;
            this.RunPassesButton.Click += new System.EventHandler(this.RunPassesButton_Click);
            // 
            // SelectPassDownButton
            // 
            this.SelectPassDownButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.SelectPassDownButton.Location = new System.Drawing.Point(706, 748);
            this.SelectPassDownButton.Margin = new System.Windows.Forms.Padding(4);
            this.SelectPassDownButton.Name = "SelectPassDownButton";
            this.SelectPassDownButton.Size = new System.Drawing.Size(146, 48);
            this.SelectPassDownButton.TabIndex = 5;
            this.SelectPassDownButton.Text = "Swap Down";
            this.SelectPassDownButton.UseVisualStyleBackColor = true;
            this.SelectPassDownButton.Click += new System.EventHandler(this.SelectPassDownButton_Click);
            // 
            // SelectPassUpButton
            // 
            this.SelectPassUpButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.SelectPassUpButton.Location = new System.Drawing.Point(560, 748);
            this.SelectPassUpButton.Margin = new System.Windows.Forms.Padding(4);
            this.SelectPassUpButton.Name = "SelectPassUpButton";
            this.SelectPassUpButton.Size = new System.Drawing.Size(138, 48);
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
            this.SelectedPassesBox.ItemHeight = 25;
            this.SelectedPassesBox.Location = new System.Drawing.Point(560, 58);
            this.SelectedPassesBox.Margin = new System.Windows.Forms.Padding(4);
            this.SelectedPassesBox.Name = "SelectedPassesBox";
            this.SelectedPassesBox.Size = new System.Drawing.Size(410, 654);
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
            this.copyAllToolStripMenuItem});
            this.PassesContextMenu.Name = "PassesContextMenu";
            this.PassesContextMenu.Size = new System.Drawing.Size(238, 124);
            // 
            // PassPropertiesMenuItem
            // 
            this.PassPropertiesMenuItem.Name = "PassPropertiesMenuItem";
            this.PassPropertiesMenuItem.Size = new System.Drawing.Size(237, 38);
            this.PassPropertiesMenuItem.Text = "&Properties...";
            this.PassPropertiesMenuItem.Click += new System.EventHandler(this.PassPropertiesMenuItem_Click);
            // 
            // toolStripMenuItem5
            // 
            this.toolStripMenuItem5.Name = "toolStripMenuItem5";
            this.toolStripMenuItem5.Size = new System.Drawing.Size(234, 6);
            // 
            // copyToolStripMenuItem1
            // 
            this.copyToolStripMenuItem1.Name = "copyToolStripMenuItem1";
            this.copyToolStripMenuItem1.Size = new System.Drawing.Size(237, 38);
            this.copyToolStripMenuItem1.Text = "&Copy";
            this.copyToolStripMenuItem1.Click += new System.EventHandler(this.copyToolStripMenuItem_Click);
            // 
            // copyAllToolStripMenuItem
            // 
            this.copyAllToolStripMenuItem.Name = "copyAllToolStripMenuItem";
            this.copyAllToolStripMenuItem.Size = new System.Drawing.Size(237, 38);
            this.copyAllToolStripMenuItem.Text = "Copy &All";
            this.copyAllToolStripMenuItem.Click += new System.EventHandler(this.copyAllToolStripMenuItem_Click);
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(554, 15);
            this.label2.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(95, 25);
            this.label2.TabIndex = 2;
            this.label2.Text = "&Pipeline:";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(8, 15);
            this.label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(384, 25);
            this.label1.TabIndex = 1;
            this.label1.Text = "&Available Passes (double-click to add):";
            // 
            // AvailablePassesBox
            // 
            this.AvailablePassesBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.AvailablePassesBox.FormattingEnabled = true;
            this.AvailablePassesBox.ItemHeight = 25;
            this.AvailablePassesBox.Location = new System.Drawing.Point(14, 58);
            this.AvailablePassesBox.Margin = new System.Windows.Forms.Padding(4);
            this.AvailablePassesBox.Name = "AvailablePassesBox";
            this.AvailablePassesBox.SelectionMode = System.Windows.Forms.SelectionMode.MultiExtended;
            this.AvailablePassesBox.Size = new System.Drawing.Size(534, 654);
            this.AvailablePassesBox.TabIndex = 0;
            this.AvailablePassesBox.DoubleClick += new System.EventHandler(this.AvailablePassesBox_DoubleClick);
            // 
            // TopSplitContainer
            // 
            this.TopSplitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TopSplitContainer.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
            this.TopSplitContainer.Location = new System.Drawing.Point(0, 40);
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
            this.TopSplitContainer.Size = new System.Drawing.Size(1568, 1017);
            this.TopSplitContainer.SplitterDistance = 800;
            this.TopSplitContainer.TabIndex = 3;
            // 
            // OutputTabControl
            // 
            this.OutputTabControl.Controls.Add(this.RenderLogTabPage);
            this.OutputTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.OutputTabControl.Location = new System.Drawing.Point(0, 0);
            this.OutputTabControl.Name = "OutputTabControl";
            this.OutputTabControl.SelectedIndex = 0;
            this.OutputTabControl.Size = new System.Drawing.Size(150, 46);
            this.OutputTabControl.TabIndex = 0;
            // 
            // RenderLogTabPage
            // 
            this.RenderLogTabPage.Controls.Add(this.RenderLogBox);
            this.RenderLogTabPage.Location = new System.Drawing.Point(8, 39);
            this.RenderLogTabPage.Name = "RenderLogTabPage";
            this.RenderLogTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.RenderLogTabPage.Size = new System.Drawing.Size(1552, 166);
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
            this.RenderLogBox.Size = new System.Drawing.Size(1546, 160);
            this.RenderLogBox.TabIndex = 0;
            this.RenderLogBox.WordWrap = false;
            // 
            // EditorForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(12F, 25F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1568, 1079);
            this.Controls.Add(this.TopSplitContainer);
            this.Controls.Add(this.TheStatusStrip);
            this.Controls.Add(this.TheMenuStrip);
            this.MainMenuStrip = this.TheMenuStrip;
            this.Margin = new System.Windows.Forms.Padding(4);
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
        private System.Windows.Forms.TabControl AnalysisTabControl;
        private System.Windows.Forms.TabPage DisassemblyTabPage;
        private System.Windows.Forms.ToolStripMenuItem editToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem viewToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem NewToolStripMenuItem;
        private System.Windows.Forms.RichTextBox DisassemblyTextBox;
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
        private System.Windows.Forms.TabPage ASTTabPage;
        private System.Windows.Forms.RichTextBox ASTDumpBox;
        private System.Windows.Forms.TabPage OptimizerTabPage;
        private System.Windows.Forms.ListBox AvailablePassesBox;
        private System.Windows.Forms.Button SelectPassDownButton;
        private System.Windows.Forms.Button SelectPassUpButton;
        private System.Windows.Forms.ListBox SelectedPassesBox;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button RunPassesButton;
        private System.Windows.Forms.Button AddPrintModuleButton;
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
        private System.Windows.Forms.Button ResetDefaultPassesButton;
        private System.Windows.Forms.CheckBox AnalyzeCheckBox;
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
    }
}