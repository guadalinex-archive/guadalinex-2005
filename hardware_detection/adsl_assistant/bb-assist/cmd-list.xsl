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

  <xsl:variable name="tty" select="//serial_params/@tty"/>
  <xsl:variable name="baudrate" select="//serial_params/@baudrate"/>
  <xsl:variable name="bits" select="//serial_params/@bits"/>
  <xsl:variable name="parity" select="//serial_params/@parity"/>
  <xsl:variable name="stopbits" select="//serial_params/@stopbits"/>
  <xsl:variable name="xonxoff" select="//serial_params/@xonxoff"/>  
  <xsl:variable name="rtscts" select="//serial_params/@rtscts"/>

  <xsl:variable name="passwd1" select="//passwd1"/>
  <xsl:variable name="passwd2" select="//passwd2"/>
  <xsl:variable name="dhcp" select="//dhcp"/>
  <xsl:variable name="dhcp_ip_start" select="//dhcp_ip_start"/>
  <xsl:variable name="dhcp_ip_end" select="//dhcp_ip_end"/>
  <xsl:variable name="mask" select="//mask"/>
  <xsl:variable name="ip_lan_router" select="//ip_lan_router"/>
  <xsl:variable name="dns1" select="//dns1"/>
  <xsl:variable name="dns2" select="//dns2"/>
  <xsl:variable name="mod_conf" select="//mod_conf"/>
  <xsl:variable name="PPPuser" select="//PPPuser"/>
  <xsl:variable name="PPPpasswd" select="//PPPpasswd"/>
  <xsl:variable name="usuIP" select="//usuIP"/>
  <xsl:variable name="usuMask" select="//usuMask"/>
  <xsl:variable name="gestIP" select="//gestIP"/>
  <xsl:variable name="gestMask" select="//gestMask"/>

  <!-- Common Vars -->

  <initial_func><xsl:value-of select="$initial_func"/></initial_func>
  <default_timeout><xsl:value-of select="$default_timeout"/></default_timeout>
  <send_delay><xsl:value-of select="$send_delay"/></send_delay>

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
    
    <cmd_func id='confDinamic3com'>
        <cmd send='Set vc internet network_service pppoec send_name {$PPPuser} send_password {$PPPpasswd}' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet local_ip_address 255.255.255.255' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet address_selection negotiate' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='confStatic3com'>
        <cmd send='Set vc internet network_service rfc_1483' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet local_ip_address {$usuIP}' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet remote_ip_address {$gestIP}/{$gestMask}' exp_ok='>' err='CLI - '/>
    </cmd_func>

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


	<!-- // A partir de aqui vuelve a ser comun -->
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
        <cmd send='set dhcp server start_address {$dhcp_ip_start} end_address {$dhcp_ip_end} mask {$mask} router {$ip_lan_router} dns1 {$dns1} dns2 {$dns2} lease 60' exp_ok='>'/>
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

      <cmd send='Add ip network LAN interface eth:1 address {$ip_lan_router}/{$mask} frame ethernet_ii enable yes' exp_ok='>' err='CLI - '/>
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
      <cmd exp_ok="" send="exit"/>
      <cmd call='speedNegEff5660'/>
    </cmd_func>
    
    <!-- Efficient SpeedStream 5660 - Passwd chg -->
    
    <cmd_func id='chgPassEff5660'>
      <cmd call='authEff5660'/>
      <!-- FIXME -->
    </cmd_func>

  </xsl:if>
</cmd_list>
</xsl:template>
</xsl:stylesheet>

