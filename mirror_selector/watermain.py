#!/usr/bin/python
# -*- coding: iso-8859-1 -*-

# watermain 1.0
# author: Alfonso E.M. alfonso@el-magnifico.org
# date: 17/Aug/2005

import pygtk
pygtk.require ('2.0')
import gtk
import gtk.glade
import os,commands,sys

class mirrors:
	filename=""
	url=""
	def __init__(self,filename,url):
		"""
		List of mirrors in a special format

		It's loaded from a file and stored in a listore object
		used in the application gui
		"""

		self.filename=filename
		self.url=url
        	self.liststore = gtk.ListStore(str,str,str,str)
		
		self.reload()



	def reload(self):

        	self.liststore.clear()

		try:
			list=open(self.filename)
		except:
			print("ERROR: mirrors.list not available")

		else:
			title=""
			urls=""
			id="none"
			mustreadurls=False
			while 1:
				line=list.readline()
				if line == "": 
					break
				if line[:6] == "#TITLE":
					title = line[7:].replace("\n","")
					mustreadurls=True
				elif line[:3] == "#ID":
					id = line[4:].replace("\n","")
				elif line[:4] == "#END":
					if id == sourceslist.id:
						icon=gtk.STOCK_YES
					else:
						icon=gtk.STOCK_NO
					i=self.liststore.append([icon,title,urls,id])
					title=""
					urls=""
					id=""
					mustreadurls=False
				else:
					if mustreadurls:
						urls = urls + line
	
			list.close


class sourceslist:
	id=""
	filename="/etc/apt/sources.list"
	def __init__(self,filename):
		"""
		The sources.list used by apt commands

		It's loaded from a file and rewrited whenever the user 
		selects a new mirror in the gui
		"""

		self.filename=filename

		try:
			list=open(self.filename)
		except:
			print "ERROR: "+self.filename+" not readable"
		else:
			self.id=""
			while 1:
				line=list.readline()
				if line == "": 
					break
				if line[:3] == "#ID":
					self.id = line[4:].replace("\n","")
			list.close
			
	def write(self,title,id,urls):

		tmpfilename=self.filename+".tmp"
		backupfilename=self.filename+".bak"

		try:
			oldfile=open(self.filename)
		except:
			sys.exit("ERROR: "+self.filename+" not readable")
			return
		try:
			newfile=open(tmpfilename,"w")
		except:
			sys.exit("ERROR: "+self.filename+".tmp not writable")
			return

		newfile.write("#TITLE:"+title+"\n")
		newfile.write("#ID:"+id+"\n")
		newfile.write("#INFO:No edite a mano las líneas entre TITLE y END. Mejor use el comando watermain.\n\n")
		newfile.write(urls)
		newfile.write("#END\n")

		mustwrite=True
		while 1:
			line=oldfile.readline()
			if line == "": 
				break
			if line[:6] == "#TITLE":
				mustwrite=False
			elif line[:4] == "#END":
				mustwrite=True
			elif mustwrite:
				newfile.write(line)

		oldfile.close
		newfile.close
	
		os.rename(self.filename,backupfilename)
		os.rename(tmpfilename,self.filename)

		return

class appgui:
	def __init__(self):
		"""
		In this init the main window is displayed
		"""
		dic = {
			 "on_bt_quit_clicked" : (gtk.mainquit),
		         "on_window1_destroy" : (gtk.mainquit), 
		         "on_treeview1_cursor_changed" : self.treeview1_cursor_changed, 
		         "on_bt_ok_clicked" : self.bt_ok_clicked, 
		         "on_bt_update_clicked" : self.bt_update_clicked 
		}

		self.xml = gtk.glade.XML("watermain.glade")
		self.xml.signal_autoconnect (dic)
		self.treeview = self.xml.get_widget('treeview1')
		self.window = self.xml.get_widget('window1')
		self.window.set_size_request(600,250)
		self.treeview.set_rules_hint(True)

		self.treeview.set_model(model=mirrors.liststore)

	        # create the TreeViewColumn to display the data
	        self.column = gtk.TreeViewColumn('Mirrors')

	        # add tvcolumn to treeview
       		self.treeview.append_column(self.column)

	        # create a CellRendererText to render the data
	        self.cellicon = gtk.CellRendererPixbuf()
	        self.cell = gtk.CellRendererText()
		self.cell.set_property('single-paragraph-mode',True)

        	# add the cell to the column
        	self.column.pack_start(self.cellicon, False)
        	self.column.pack_start(self.cell, True)

	        # set the cell "text" attribute to column 0 - retrieve text
 	        # from that column in treestore
       		self.column.set_attributes(self.cellicon, stock_id=0)
       		self.column.add_attribute(self.cell, 'markup',1)

		return

#	def bt_ok_deactivate(self):
#		widget=self.xml.get_widget('bt_ok')
#		widget.set_sensitive(False)


	def bt_ok_clicked(self,widget):
		widget.set_sensitive(False)
		selection=app.treeview.get_selection()
		(model,iter)=selection.get_selected()
		if iter:
			title=mirrors.liststore.get_value(iter,1)
			urls=mirrors.liststore.get_value(iter,2)
			id=mirrors.liststore.get_value(iter,3)
			sourceslist.write(title,id,urls)
		sys.exit(0)

	def bt_update_clicked(self,widget):
		widget.set_sensitive(False)
		tmpfilename=mirrors.filename+".tmp"
		status,output=commands.getstatusoutput("wget --quiet --tries=3 --user-agent=watermain --output-document="+tmpfilename +" "+mirrors.url)
		if status == 0:
			os.rename(tmpfilename,mirrors.filename)
		else:
			print output
			sys.exit("ERROR: No es posible descargar una nueva lista")

		widget.set_sensitive(True)
		mirrors.reload()
		return

	def treeview1_cursor_changed(self,treeview):
		widget=self.xml.get_widget('bt_ok')
		widget.set_sensitive(True)
		return


sourceslist=sourceslist("/etc/apt/sources.list")
mirrors=mirrors("/etc/watermain.list","http://www.guadalinex.org/mirrors.list")
app=appgui()
  
gtk.main()
