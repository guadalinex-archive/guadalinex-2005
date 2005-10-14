<?xml version='1.0' encoding='iso-8859-1'?>
<xsl:stylesheet version="1.0"
 xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" indent="yes" encoding="iso-8859-1"/>
<xsl:template match="/">

<cmd_list>

  <!-- Vars -->

  <xsl:variable name="model" select="//model"/>
  <xsl:variable name="initial_func" select="//initial_func"/>
  <xsl:variable name="default_timeout" select="//default_timeout"/>
  <xsl:variable name="send_delay" select="//send_delay"/>

  <xsl:variable name="dev" select="//eth_params/@dev"/>
  <xsl:variable name="ip" select="//eth_params/@ip"/>
  <xsl:variable name="port" select="//eth_params/@port"/>

  <xsl:variable name="tty" select="//serial_params/@tty"/>
  <xsl:variable name="baudrate" select="//serial_params/@baudrate"/>
  <xsl:variable name="bits" select="//serial_params/@bits"/>
  <xsl:variable name="parity" select="//serial_params/@parity"/>
  <xsl:variable name="stopbits" select="//serial_params/@stopbits"/>
  <xsl:variable name="xonxoff" select="//serial_params/@xonxoff"/>  
  <xsl:variable name="rtscts" select="//serial_params/@rtscts"/>

  <xsl:variable name="passwd1" select="//passwd1"/>
  <xsl:variable name="passwd2" select="//passwd2"/>

  <xsl:variable name="PPPuser" select="//PPPuser"/>
  <xsl:variable name="PPPpasswd" select="//PPPpasswd"/>

  <xsl:variable name="mod_conf" select="//mod_conf"/>
  <xsl:variable name="remote_ip" select="//remote_ip"/>
  <xsl:variable name="remote_mask" select="//remote_mask"/>
  <xsl:variable name="ext_ip_router" select="//ext_ip_router"/>
  <xsl:variable name="ext_mask_router" select="//ext_mask_router"/>
  <xsl:variable name="int_ip_router" select="//int_ip_router"/>
  <xsl:variable name="int_mask_router" select="//int_mask_router"/>
  <xsl:variable name="dhcp" select="//dhcp"/>
  <xsl:variable name="dhcp_ip_start" select="//dhcp_ip_start"/>
  <xsl:variable name="dhcp_ip_end" select="//dhcp_ip_end"/>
  <xsl:variable name="dhcp_mask" select="//dhcp_mask"/>
  <xsl:variable name="dhcp_ip_gw" select="//dhcp_ip_gw"/>
  <xsl:variable name="dns1" select="//dns1"/>
  <xsl:variable name="dns2" select="//dns2"/>

  <!-- Common Vars -->

  <initial_func><xsl:value-of select="$initial_func"/></initial_func>
  <default_timeout><xsl:value-of select="$default_timeout"/></default_timeout>
  <send_delay><xsl:value-of select="$send_delay"/></send_delay>

  <eth_params dev='{$dev}' ip='{$ip}' port='{$port}'/>

  <serial_params 
      tty='{$tty}' 
      baudrate='{$baudrate}' 
      bits='{$bits}' 
      parity='{$parity}'
      stopbits='{$stopbits}'
      xonxoff='{$xonxoff}'
      rtscts='{$rtscts}'/>

  <!-- Common Functions -->

  <cmd_func id='__ok__'><cmd return='0'/></cmd_func>
  <cmd_func id='__exit__'><cmd return='1'/></cmd_func>
  <cmd_func id='__eof__'><cmd return='2'/></cmd_func>
  <cmd_func id='__timeout__'><cmd return='3'/></cmd_func>


  <xsl:if test = "$model='3Com OfficeConnect 812'">

    <!-- 3Com OfficeConnect 812 -->

    <!-- 3Com OfficeConnect 812 - Auth -->
    
    <cmd_func id='auth3com'>
      <cmd send='' exp_ok='(CLI -|>)'>
        <expect_list>
          <expect out='Password:'>
	    <cmd send='{$passwd1}' exp_ok='>'>
	      <expect_list>
		<expect out='Password:'>
		  <cmd return='4'/> <!-- Wrong passwd -->
		</expect>
	      </expect_list>
	    </cmd>
	  </expect>
	</expect_list>
      </cmd>
    </cmd_func>

    <!-- 3Com OfficeConnect 812 - Test Access -->

    <cmd_func id='verif3com'>
      <cmd call='auth3com'/>
      <cmd send='exit cli' exp_ok='Password:' err='CLI - '/>
    </cmd_func>
    
    <!-- 3Com OfficeConnect 812 - Passwd chg -->
    
    <cmd_func id='chgPass3com'>
      <cmd call='auth3com'/>
      
      <cmd send='delete user {$passwd1}' exp_ok='>' err='CLI - '/>
      <cmd send='enable command password {$passwd2}' exp_ok='>' err='CLI - '/>
      <cmd send='add user {$passwd2} password {$passwd2}' exp_ok='>' err='CLI - '/>
      <cmd send='save all' exp_ok='>' err='CLI - '/>
      <cmd send='exit cli' exp_ok='Password:' err='CLI - '/>
      <cmd return='0'/>
    </cmd_func>

    <!-- 3Com OfficeConnect 812 - List Configuration -->

    <cmd_func id='listConf3com'>
      <cmd call='auth3com'/>
      <cmd send='show ip network LAN settings' exp_ok='>' err='CLI - '/>
      <cmd send='show vc internet settings' exp_ok='stop output:' err='CLI - '/>
      <cmd send='' exp_ok='>' err='CLI - '/>
      <cmd send='show dhcp server settings' exp_ok='>' err='CLI - '/>
      <cmd send='show system' exp_ok='>' err='CLI - '/>
      <cmd send='show ethernet settings' exp_ok='>' err='CLI - '/>
      <cmd send='list vc' exp_ok='>' err='CLI - '/>
      <cmd send='show adsl transceiver_status' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <!-- 3Com OfficeConnect 812 - Configuration -->
    
    <cmd_func id='confDinamic3com'>
        <cmd send='Set vc internet network_service pppoec send_name {$PPPuser} send_password {$PPPpasswd}' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet local_ip_address 255.255.255.255' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet address_selection negotiate' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='confStatic3com'>
        <cmd send='Set vc internet network_service rfc_1483' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet local_ip_address {$ext_ip_router}' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet remote_ip_address {$remote_ip}/{$remote_mask}' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='errServDhcp3com'><cmd return='5'/></cmd_func>

    <cmd_func id='confFilter3com'>
      <!-- //Filtro para bloquear puerto 21, 23, 80 desde internet, 
	   a todo el mundo excepto a una subred -->
      <cmd send='capture text_file filtro' exp_ok=''/>
      <cmd send='#filter' exp_ok=''/>
      <cmd send='IP:' exp_ok=''/>
      <cmd send='1 ACCEPT src-addr=193.152.37.192/28;' exp_ok=''/>
      <cmd send='2 REJECT tcp-dst-port=21;' exp_ok=''/>
      <cmd send='3 REJECT tcp-dst-port=80;' exp_ok=''/>
      <cmd send='4 REJECT tcp-dst-port=23;' exp_ok=''/>
      <cmd send='\004' exp_ok='>'/>
      <cmd send='add filter filtro' exp_ok='>' err='CLI - '/>
      <cmd send='set interface atm:1 input_filter filtro' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='confBridge3com'>
        <cmd send='Set vc internet ip disable ipx disable bridging enable' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet network_service rfc_1483' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet atm vpi 8 vci 32 category_of_service unspecified pcr 0' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet mac_routing disable' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet nat_option disable' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='confNoBridge3com'>
	<cmd send='Set vc internet ip enable ipx disable bridging disable' exp_ok='>' err='CLI - '/>
	<xsl:if test = "$mod_conf='multidinamic'"><cmd call='confDinamic3com'/></xsl:if>
	<xsl:if test = "$mod_conf='monodinamic'"><cmd call='confDinamic3com'/></xsl:if>
	<xsl:if test = "$mod_conf='monostatic'"><cmd call='confStatic3com'/></xsl:if>
	<xsl:if test = "$mod_conf='multistatic'"><cmd call='confStatic3com'/></xsl:if>

	<cmd send='Set vc internet atm vpi 8 vci 32 category_of_service unspecified pcr 0' exp_ok='>' err='CLI - '/>
	<cmd send='Set vc internet mac_routing disable' exp_ok='>' err='CLI - '/>
	
	<!-- NAT -->
	<xsl:if test = "$mod_conf='multistatic'">
	  <cmd send='Set vc internet nat_option enable' exp_ok='>' err='CLI - '/>
        </xsl:if>
	<xsl:if test = "$mod_conf='multidinamic'">
	  <cmd send='Set vc internet nat_option enable' exp_ok='>' err='CLI - '/>
        </xsl:if>

	<!-- No NAT -->
	<xsl:if test = "$mod_conf='monostatic'">
	  <cmd send='Set vc internet nat_option disable' exp_ok='>' err='CLI - '/>
        </xsl:if>
	<xsl:if test = "$mod_conf='monodinamic'">
	  <cmd send='Set vc internet nat_option disable' exp_ok='>' err='CLI - '/>
        </xsl:if>

	<cmd send='Set vc internet default_route_option enable' exp_ok='>' err='CLI - '/>
	<!-- FIXME: Set vc internet nat_default_address -->
    </cmd_func>

    <cmd_func id='conf3com'>
      <cmd call='auth3com'/>
      <!-- Primero se reseta a los valores de fabrica: -->
      <cmd send='delete configuration' exp_ok='No\/Yes' err='CLI - '/>
      <cmd send='y' exp_ok='OfficeConnect Quick Setup?' timeout='40'/>
      <cmd send='n' exp_ok='Starting line test ...'/>
      <cmd send='' exp_ok='>'/>

      <!-- // Se configura en multipuesto estatico -->

      <cmd send='set system name telefonica' exp_ok='>' err='CLI - '/>
      <!-- // Si fija la clave de acceso al modem -->
      <cmd send='enable command password {$passwd1}' exp_ok='>' err='CLI - '/>
      <xsl:if test = "$dhcp='True'">
        <!-- // Si se desea DHCP se escribe -->
        <cmd send='set dhcp mode server' exp_ok='>'/>
        <cmd send='set dhcp server start_address {$dhcp_ip_start} end_address {$dhcp_ip_end} mask {$dhcp_mask} router {$dhcp_ip_gw} dns1 {$dns1} dns2 {$dns2} lease 60' exp_ok='>' err='CLI - ' on_except='errServDhcp3com' />
      </xsl:if>
      <xsl:if test = "$dhcp='False'">
        <!-- // Si no se desea DHCP se escribe -->
        <cmd send='set dhcp mode disabled' exp_ok='>' err='CLI - '/>
      </xsl:if>
      
      <!-- // Bridge o NAT -->
      <xsl:if test = "$mod_conf='monostatic'">
        <cmd send='add bridge network internet' exp_ok='>' err='CLI - '/>
        <cmd send='disable ip forwarding' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <xsl:if test = "$mod_conf='monodinamic'">
        <cmd send='add bridge network internet' exp_ok='>' err='CLI - '/>
        <cmd send='disable ip forwarding' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <xsl:if test = "$mod_conf='multistatic'">
	<cmd send='enable ip forwarding' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <xsl:if test = "$mod_conf='multidinamic'">
	<cmd send='enable ip forwarding' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <cmd send='Add ip network LAN interface eth:1 address {$int_ip_router}/{$int_mask_router} frame ethernet_ii enable yes' exp_ok='>' err='CLI - '/>
      <cmd send='add vc internet' exp_ok='>' err='CLI - '/>

      <xsl:if test = "$mod_conf='monodinamic'"><cmd call='confBridge3com'/></xsl:if>
      <xsl:if test = "$mod_conf='monostatic'"><cmd call='confBridge3com'/></xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'"><cmd call='confNoBridge3com'/></xsl:if>
      <xsl:if test = "$mod_conf='multistatic'"><cmd call='confNoBridge3com'/></xsl:if>

      <cmd send='disable network service httpd' exp_ok='>' err='CLI - '/>
      <cmd send='set adsl open ansi' exp_ok='>' err='CLI - '/>

      <!-- // Gestion de usuario y clave. Suponemos que usuario=clave -->
      <cmd send='add user {$passwd1} password {$passwd1}' exp_ok='>' err='CLI - '/>
      <cmd send='set user {$passwd1} session_timeout 180' exp_ok='>' err='CLI - '/>
      
      <!-- NAT -->
      <xsl:if test = "$mod_conf='multistatic'"><cmd call='confFilter3com'/></xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'"><cmd call='confFilter3com'/></xsl:if>

      <cmd send='set command login_required yes' exp_ok='>' err='CLI - '/>
      <cmd send='enable security_option remote_user administration' exp_ok='>' err='CLI - '/>

      <!-- //Ejemplo de mapeo -->
      <!-- FIXME
           <cmd send='add nat tcp vc internet private_add [IP PC. Ej:192.168.1.33] private_port [Puerto interno, Ej: 21] public_port [Puerto publico.Ej: 21]' exp_ok='>' err='CLI - '/>
      -->

      <cmd send='set vc internet intelligent_nat_option disable' exp_ok='>' err='CLI - '/>
      <cmd send='disable dns' exp_ok='>' err='CLI - '/>
      <cmd send='disable network service tftpd' exp_ok='>' err='CLI - '/>
      <cmd send='disable security_option snmp user_access' exp_ok='>' err='CLI - '/>
      <cmd send='disable ip rip' exp_ok='>' err='CLI - '/>
      <cmd send='enable vc internet' exp_ok='>' err='CLI - '/>
      <cmd send='save all' exp_ok='>' err='CLI - '/>
      <cmd send='exit cli' exp_ok='>' err='CLI - '/>
    </cmd_func>
  </xsl:if>

  <xsl:if test = "$model='Efficient SpeedStream 5660'">    

    <!-- Efficient SpeedStream 5660 -->

    <!-- Efficient SpeedStream 5660 - Auth -->

    <cmd_func id='speedNegEff5660'>
      <!-- SpeedStream 5660 expects 5 CR/LF after Password
	   for negociate the serial speed -->
      <cmd send='\r' exp_ok=''/>
      <cmd send='\r' exp_ok=''/>
      <cmd send='\r' exp_ok=''/>
      <cmd send='\r' exp_ok=''/>
      <cmd send='\r' exp_ok=''/>
    </cmd_func>
   
    <cmd_func id='authEff5660'>
      <!-- The login of this device is very unrealiable,
	   the we try it several times before return error -->
      <cmd call='speedNegEff5660'/>
      <cmd send='\r' exp_ok='Command->'>
        <expect_list>
	  <expect out='Password'>
	    <cmd send='{$passwd1}\r' exp_ok='Command->'>
	      <expect_list>
		<expect out='Password:'>
		  <cmd send='{$passwd1}\r' exp_ok='Command->'>
		    <expect_list>
		      <expect out='Password:'>
			<cmd send='{$passwd1}\r' exp_ok='Command->'>
			  <expect_list>
			    <expect out='Password:'>
			      <cmd send='{$passwd1}\r' exp_ok='Command->'>
				<expect_list>
				  <expect out='Password:'>
				    <cmd send='{$passwd1}\r' exp_ok='Command->'>
				      <expect_list>
					<expect out='Password:'>
					  <cmd send='{$passwd1}\r' exp_ok='Command->'>
					    <expect_list>
					      <expect out='Password:'>
						<!-- Wrong passwd -->
						<cmd return='4'/> 
					      </expect>
					    </expect_list>
					  </cmd>
					</expect>
				      </expect_list>
				    </cmd>
				  </expect>
				</expect_list>
			      </cmd>
			    </expect>
			  </expect_list>
			</cmd>
		      </expect>
		    </expect_list>
		  </cmd>
		</expect>
	      </expect_list>
	    </cmd>
	  </expect>
	</expect_list>
      </cmd>
    </cmd_func>
    
    <!-- Efficient SpeedStream 5660 - Test Access -->

    <cmd_func id='verifEff5660'>
      <cmd call='authEff5660'/>
      <cmd send='exit' exp_ok=""/>
      <cmd call='speedNegEff5660'/>
    </cmd_func>
    
    <!-- Efficient SpeedStream 5660 - Passwd chg -->
    
    <cmd_func id='chgPassEff5660'>
      <cmd call='authEff5660'/>
      <cmd send='set password' exp_ok='Old password :'/>
      <cmd send='{$passwd1}' exp_ok='New password :'/>
      <cmd send='{$passwd1}' exp_ok='New password :'/>
      <cmd send='{$passwd1}' exp_ok='Password updated'/>
      <cmd send='' exp_ok='Command->'/>
    </cmd_func>

    <cmd_func id='confBridgeEff5660'>
      <cmd send='set vc 8 32 llc max' exp_ok='Command->'/>
    </cmd_func>

    <cmd_func id='confNoBridgeEff5660'>
      <cmd send='Set dns disable' exp_ok='Command->'/>
      
      <xsl:if test = "$mod_conf='multidinamic'">
	<cmd send='set vc ppp 8 32 max 0.0.0.0' exp_ok='Command->'/>
	<cmd send='set pppauth {$PPPuser} {$PPPpasswd}' exp_ok='Command->'/>
      </xsl:if>

      <xsl:if test = "$mod_conf='multistatic'">
	<cmd send='set vc 1483r 8 32 llc max {$ext_ip_router} {$ext_mask_router}' exp_ok='Command->'/>
	<cmd send='set ipgateway {$remote_ip}' exp_ok='Command->'/>      
      </xsl:if>

    </cmd_func>      

    <cmd_func id='confEff5660'>
      <cmd call='authEff5660'/>
      <!-- // Primero lo resetamos -->
      <cmd send='default all' exp_ok='Command->'/>

      <!-- // Empezamos a configurar -->
      <cmd send='set bridge disable' exp_ok='Command->'/>
      <!-- FIXME: Password must be at least 8 chars -->
      <cmd send='set password' exp_ok='Old password :'/>
      <cmd send='{$passwd1}' exp_ok='New password :'/>
      <cmd send='{$passwd1}' exp_ok='New password :'/>
      <cmd send='{$passwd1}' exp_ok='Password updated'/>
      <cmd send='' exp_ok='Command->'/>

      <cmd send='set ethip {$int_ip_router} {$int_mask_router}' exp_ok='Command->'/>
      <cmd send='set hostname telefonica' exp_ok='Command->'/>
      <cmd send='set ethcfg full' exp_ok='Command->'/>
      <cmd send='set serverport HTTP 80 disable' exp_ok='Command->'/>
      <cmd send='set serverport FTP 21 disable' exp_ok='Command->'/>
      <cmd send='set serverport SNMP 161 disable' exp_ok='Command->'/>
      <cmd send='set serverport telnet 23 enable' exp_ok='Command->'/>

      <cmd return='6'/>

      <!-- // Si no hay servidor DHCP -->

      <xsl:if test = "$dhcp='False'">
	<cmd send='set dhcp disable' exp_ok='Command->'/>
      </xsl:if>

      <cmd send='set dhcp server start_address {$dhcp_ip_start} end_address {$dhcp_ip_end} mask {$mask} router {$ip_lan_router} dns1 {$dns1} dns2 {$dns2} lease 60' exp_ok='>'/>

      <xsl:if test = "$dhcp='True'">
	<cmd send='set dhcpcfg server {$dhcp_ip_start} {$mask} {$dhcp_ip_end} {$ip_lan_router} {$dns1} {$dns2} 0.0.0.0 0.0.0.0 megavia none server 60' exp_ok='Command->'/>
	<cmd send='set dhcp enable' exp_ok='Command->'/>
      </xsl:if>

      <!-- // A partir de aqui vuelve a ser comun -->
      <cmd send='set ripcfg none' exp_ok='Command->'/>
      <cmd send='set napt enable' exp_ok='Command->'/>

      <!-- //Filtro para bloquear puerto 23 desde internet, a todo el mundo excepto a una subred -->
      <cmd send='set ipfiltercfg 1 in permit all [Ip de subred con permisos de acceso al puerto 23. Ej: 80.80.23.0] [Mascara. Ej:255.255.255.240] any any' exp_ok='Command->'/>
      <cmd send='set ipfiltercfg 2 in deny tcp any any any eq 23 yes' exp_ok='Command->'/>
      <cmd send='set ipfiltercfg 3 in permit all any any' exp_ok='Command->'/>
      <cmd send='set ipfilter enable' exp_ok='Command->'/>

      <!-- //Hacemos un mapeo -->
      <cmd send='set naptserver tcp [Puerto publico.Ej: 21] [IP PC. Ej:192.168.1.33]' exp_ok='Command->'/>

      <!-- // Reiniciamos para aplica cambios -->
      <cmd send='reboot' exp_ok='y,n'/>
      <cmd send='y' exp_ok='System rebooting as requested'/>
    </cmd_func>
  </xsl:if>
</cmd_list>
</xsl:template>
</xsl:stylesheet>

