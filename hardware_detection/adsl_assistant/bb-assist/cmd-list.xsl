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

  <xsl:variable name="vpi" select="//vpi"/>
  <xsl:variable name="vci" select="//vci"/>

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
  <xsl:variable name="dhcp_ip_net" select="//dhcp_ip_net"/>
  <xsl:variable name="dhcp_num_hosts" select="//dhcp_num_hosts"/>
  <xsl:variable name="dhcp_mask" select="//dhcp_mask"/>
  <xsl:variable name="dhcp_ip_gw" select="//dhcp_ip_gw"/>
  <xsl:variable name="dns1" select="//dns1"/>
  <xsl:variable name="dns2" select="//dns2"/>
  <xsl:variable name="net_computer" select="//net_computer"/>
  <xsl:variable name="mask_computer" select="//mask_computer"/>

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
          </expect>
          <expect out='Do you want to continue with OfficeConnect Quick Setup'>
            <cmd send='n' exp_ok='Starting line test ...'/>
            <cmd send='' exp_ok='>'/>
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
      <cmd send='\004' exp_ok='>' timeout='40'/>
      <cmd send='add filter filtro' exp_ok='>' err='CLI - '/>
      <cmd send='set interface atm:1 input_filter filtro' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='confBridge3com'>
        <cmd send='Set vc internet ip disable ipx disable bridging enable' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet network_service rfc_1483' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet atm vpi {$vpi} vci {$vci} category_of_service unspecified pcr 0' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='confNoBridge3com'>
        <cmd send='Set vc internet ip enable ipx disable bridging disable' exp_ok='>' err='CLI - '/>
        <xsl:if test = "$mod_conf='multidinamic'"><cmd call='confDinamic3com'/></xsl:if>
        <xsl:if test = "$mod_conf='monodinamic'"><cmd call='confDinamic3com'/></xsl:if>
        <xsl:if test = "$mod_conf='monostatic'"><cmd call='confStatic3com'/></xsl:if>
        <xsl:if test = "$mod_conf='multistatic'"><cmd call='confStatic3com'/></xsl:if>

        <cmd send='Set vc internet atm vpi {$vpi} vci {$vci} category_of_service unspecified pcr 0' exp_ok='>' err='CLI - '/>
        <cmd send='Set vc internet default_route_option enable' exp_ok='>' err='CLI - '/>
    </cmd_func>

    <cmd_func id='conf3com'>
      <cmd call='auth3com'/>

      <cmd send='delete configuration' exp_ok='No\/Yes' err='CLI - '/>
      <cmd send='y' exp_ok='OfficeConnect Quick Setup?' timeout='40'/>
      <cmd send='n' exp_ok='Starting line test ...'/>
      <cmd send='' exp_ok='>'/>

      <cmd send='set system name telefonica' exp_ok='>' err='CLI - '/>

      <cmd send='enable command password {$passwd1}' exp_ok='>' err='CLI - '/>

      <xsl:if test = "$dhcp='True'">
        <cmd send='set dhcp mode server' exp_ok='>'/>
        <cmd send='set dhcp server start_address {$dhcp_ip_start} end_address {$dhcp_ip_end} mask {$dhcp_mask} router {$dhcp_ip_gw} dns1 {$dns1} dns2 {$dns2} lease 120' exp_ok='>' err='CLI - ' on_except='errServDhcp3com' />
      </xsl:if>
      <xsl:if test = "$dhcp='False'">
        <cmd send='set dhcp mode disabled' exp_ok='>' err='CLI - '/>
      </xsl:if>
      
      <xsl:if test = "$mod_conf='monodinamic'">
        <cmd send='add bridge network puente' exp_ok='>' err='CLI - '/>
        <cmd send='disable ip forwarding' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <xsl:if test = "$mod_conf!='monodinamic'">
        <cmd send='enable ip forwarding' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <cmd send='Add ip network LAN interface eth:1 address {$int_ip_router}/{$int_mask_router} frame ethernet_ii enable yes' exp_ok='>' err='CLI - '/>
      <cmd send='add vc internet' exp_ok='>' err='CLI - '/>

      <xsl:if test = "$mod_conf='monodinamic'">
        <cmd call='confBridge3com'/>
        <cmd send='Set vc internet nat_option disable' exp_ok='>' err='CLI - '/>
      </xsl:if>
      <xsl:if test = "$mod_conf='monostatic'">
        <cmd send='Set vc internet nat_option disable' exp_ok='>' err='CLI - '/>
        <cmd call='confNoBridge3com'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd call='confNoBridge3com'/>
        <cmd send='Set vc internet nat_option enable' exp_ok='>' err='CLI - '/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multistatic'">
        <cmd call='confNoBridge3com'/>
        <cmd send='Set vc internet nat_option enable' exp_ok='>' err='CLI - '/>
      </xsl:if>

      <cmd send='Set vc internet mac_routing disable' exp_ok='>' err='CLI - '/>
      <cmd send='disable network service httpd' exp_ok='>' err='CLI - '/>
      <cmd send='set adsl open ansi' exp_ok='>' err='CLI - '/>

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

      <!-- Not sure 
      <cmd send='set vc internet intelligent_nat_option disable' exp_ok='>' err='CLI - '/>
      <cmd send='disable dns' exp_ok='>' err='CLI - '/>
      <cmd send='disable network service tftpd' exp_ok='>' err='CLI - '/>
      <cmd send='disable security_option snmp user_access' exp_ok='>' err='CLI - '/>
      -->
      <cmd send='enable vc internet' exp_ok='>' err='CLI - '/>
      <cmd send='save all' exp_ok='>' err='CLI - '/>
      <cmd send='exit cli' exp_ok='>' err='CLI - '/>
    </cmd_func>
    
    <cmd_func id='testDHCP3c'>   <!-- FIXME erase when finished -->
      <cmd call='auth3com'/>
      <cmd send='set dhcp mode server' exp_ok='>' err='CLI - ' on_except='errServDhcp3com' />
      <cmd send='set dhcp server start_address {$dhcp_ip_start} end_address {$dhcp_ip_end} mask {$dhcp_mask} router {$dhcp_ip_gw} dns1 {$dns1} dns2 {$dns2} lease 120' exp_ok='>' err='CLI - ' on_except='errServDhcp3com' />
      <cmd send='exit cli' exp_ok='Password:' err='CLI - '/>
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
      <cmd send='\r' exp_ok='Command->'/>
      <cmd send='exit' exp_ok=""/>
      <cmd call='speedNegEff5660'/>
    </cmd_func>
    
    <!-- Efficient SpeedStream 5660 - Passwd chg -->
    
    <cmd_func id='chgPassEff5660'>
      <cmd call='authEff5660'/>
      <cmd send='\r' exp_ok='Command->'/>
      <cmd send='set password' exp_ok='Old password :'/>
      <cmd send='{$passwd2}\r' exp_ok='New password :'/>
      <cmd send='{$passwd2}\r' exp_ok='New password :'/>
      <cmd send='{$passwd2}\r' exp_ok='Password updated'/>
      <cmd send='exit' exp_ok=""/>
      <cmd call='speedNegEff5660'/>
    </cmd_func>

    <!-- Efficient SpeedStream 5660 - Configuration -->

    <cmd_func id='confBridgeEff5660'>
      <cmd send='set vc {$vpi} {$vci} llc max' exp_ok='Command->'/>
    </cmd_func>

    <cmd_func id='confNoBridgeEff5660'>
      <cmd send='Set dns disable' exp_ok='Command->' err='(failed|for help)'/>
            
      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='set vc ppp {$vpi} {$vci} max 0.0.0.0' exp_ok='Command->' err='(failed|for help)'/>
        <cmd send='set pppauth {$PPPuser} {$PPPpasswd}' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>

      <xsl:if test = "$mod_conf!='multidinamic'">
        <cmd send='set vc 1483r {$vpi} {$vci} llc max {$ext_ip_router} {$ext_mask_router}' exp_ok='Command->' err='(failed|for help)'/>
        <cmd send='set ipgateway {$ext_ip_router}' exp_ok='(failed|Command)'/>
        <cmd send='' exp_ok='Command->'/>      
      </xsl:if>

    </cmd_func>      

    <cmd_func id='confEff5660'>
      <cmd call='authEff5660'/>

      <cmd send='default all' exp_ok='Setting VC configuration to factory defaults, reboot required'/>
      <cmd send='' exp_ok='Command->'/>

      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='set bridge disable' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>

      <!-- FIXME: Password must be at least 8 chars -->
      <cmd send='set password' exp_ok='(Old password :|New password :)' err='(failed|for help)'/>
      <cmd send='{$passwd1}\r' exp_ok='New password :'/>
      <cmd send='{$passwd1}\r' exp_ok='Password updated'>
        <expect_list>
          <expect out='New password :'>
            <cmd send='{$passwd1}\r' exp_ok='Password updated'/>
          </expect>
        </expect_list>
      </cmd>
      <cmd send='set ethip {$int_ip_router} {$int_mask_router}' exp_ok='Selected IP address is on same network as another interface' err='for help'>
        <expect_list>
          <expect out='y,n'>
            <cmd send='n' exp_ok='Command->'/>
          </expect>
        </expect_list>
      </cmd>
      <cmd send='\r' exp_ok='Command->'/>
      <cmd send='set hostname telefonica' exp_ok='Command->' err='(failed|for help)'/>
      <cmd send='\r' exp_ok='Command->'/>
      <cmd send='set ethcfg full' exp_ok='Command->' err='(failed|for help)'/>

      <xsl:if test = "$mod_conf!='monodinamic'">
        <cmd send='set serverport HTTP 80 disable' exp_ok='Command->' err='(failed|for help)'/>
        <cmd send='set serverport FTP 21 disable' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>

      <xsl:if test = "$dhcp!='True'">
        <xsl:if test = "$mod_conf!='monodinamic'">
          <cmd send='set dhcp disable' exp_ok='Command->' err='(failed|for help)'/>
        </xsl:if>
      </xsl:if>

      <xsl:if test = "$dhcp='True'">
        <cmd send='set dhcpcfg server {$dhcp_ip_start} {$dhcp_mask} {$dhcp_ip_end} {$dhcp_ip_gw} {$dns1} {$dns2} 0.0.0.0 0.0.0.0 megavia none server 120' exp_ok='Command->' err='(failed|for help)'/>
        <cmd send='set dhcp enable' exp_ok='Command->'/>
      </xsl:if>

      <xsl:if test = "$mod_conf='monodinamic'"><cmd call='confBridgeEff5660'/></xsl:if>
      <xsl:if test = "$mod_conf='monostatic'"><cmd call='confNoBridgeEff5660'/></xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'"><cmd call='confNoBridgeEff5660'/></xsl:if>
      <xsl:if test = "$mod_conf='multistatic'"><cmd call='confNoBridgeEff5660'/></xsl:if>

      <xsl:if test = "$mod_conf!='monodinamic'">
        <cmd send='set ripcfg none' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>

      <xsl:if test = "$mod_conf='monostatic'">
        <cmd send='set napt disable' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='set napt enable' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multistatic'">
        <cmd send='set napt enable' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>

      <xsl:if test = "$mod_conf!='monodinamic'">
        <cmd send='set ipfiltercfg 1 in permit all 193.152.37.192 255.255.255.240 any any' exp_ok='Command->' err='for help'/>
        <cmd send='set ipfiltercfg 2 in deny tcp any any any eq 80 yes' exp_ok='Command->' err='for help'/>
        <cmd send='set ipfiltercfg 3 in deny tcp any any any eq 21 yes' exp_ok='Command->' err='for help'/>
        <cmd send='set ipfiltercfg 4 in deny tcp any any any eq 23 yes' exp_ok='Command->' err='for help'/>
        <cmd send='set ipfiltercfg 5 in permit all any any' exp_ok='Command->' err='for help'/>
        <cmd send='set ipfilter enable' exp_ok='Command->' err='(failed|for help)'/>
      </xsl:if>

      <!-- //Hacemos un mapeo -->
      <!--
          <cmd send='set naptserver tcp 
          [Puerto publico.Ej: 21] [IP PC. Ej:192.168.1.33]' exp_ok='Command->'/>
      -->

      <cmd send='reboot' exp_ok='y,n'/>
      <cmd send='y' exp_ok='System rebooting as requested' err='(failed|for help)'/>
    </cmd_func>

    <cmd_func id='resetEff5660'>
      <cmd call='authEff5660'/>

      <cmd send='default all' exp_ok='Setting VC configuration to factory defaults, reboot required'/>
      <cmd send='\r' exp_ok='Command->'/>
      <cmd send='reboot' exp_ok='y,n'/>
      <cmd send='y' exp_ok='System rebooting as requested' err='(failed|for help)'/>
    </cmd_func>
  </xsl:if>

  <xsl:if test = "$model='Nokia M1112 ADSL Router'">

    <xsl:variable name="defNokM112err" select="'(invalid command|usage|fail on request)'"/>

    <!-- Nokia M1112 ADSL Router -->

    <!-- Nokia M1112 ADSL Router - Auth -->
    
    <cmd_func id='authNokiaM1112'>
      <cmd send='\x17\x17\r' exp_ok='>'> <!-- we are already logged -->
        <expect_list>
          <expect out='(login-id:|password:)'>
            <cmd send='{$passwd1}\r' exp_ok='>'>
              <expect_list>
                <expect out='(login-id:|password:)'>
                  <cmd send='{$passwd1}\r' exp_ok='>'>
                    <expect_list>
                      <expect out='(login-id:|password:)'> 
                        <!-- Wrong passwd (after three fails)-->
                        <cmd return='4'/> 
                      </expect>
                    </expect_list>
                  </cmd>
                </expect>
              </expect_list>
            </cmd>
          </expect>
          <expect out='\)#'>
            <cmd send='quit\r' exp_ok='>'/>
          </expect>
        </expect_list>
      </cmd>
    </cmd_func>


    <!-- Nokia M1112 ADSL Router - Test Access -->

    <cmd_func id='verifNokiaM1112'>
      <cmd call='authNokiaM1112'/>
      <cmd send='logout\r' exp_ok='login-id:' err='{$defNokM112err}'/>
    </cmd_func>

    <!-- Nokia M1112 ADSL Router - Change Passwd -->

    <cmd_func id='chgPassNokiaM1112'>
      <cmd call='authNokiaM1112'/>
      <cmd send='configure\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='password\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='admin {$passwd2}\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='quit\r' exp_ok='>' err='{$defNokM112err}'/>
      <cmd send='save config startup\r' exp_ok='>' err='{$defNokM112err}'/>
      <cmd send='logout\r' exp_ok='goodbye' err='{$defNokM112err}'/>
    </cmd_func>

    <!-- Nokia M1112 ADSL Router - Configuration -->

    <cmd_func id='confNokiaM1112'>
      <cmd call='authNokiaM1112'/>
      <cmd send='restore config default\r' exp_ok='>' err='{$defNokM112err}'/>
      <cmd send='reload\r' exp_ok='login-id' err='{$defNokM112err}'/>
      <cmd call='authNokiaM1112'/>
      <cmd send='configure\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='password\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='admin {$passwd1}\r' exp_ok='#' err='{$defNokM112err}'/>

      <xsl:if test = "$dhcp='True'">      
        <cmd send='common\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='dhcp mode server\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='dhcp address 1 {$dhcp_ip_start} {$dhcp_mask} {$dhcp_num_hosts}\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='dhcp dns 1 primary 80.58.0.33\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='dhcp dns 1 secondary 80.58.32.97\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='dhcp lease-time 1 120\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <xsl:if test = "$dhcp!='True'">
        <cmd send='common\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='no dhcp mode\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <cmd send='eth\r' exp_ok='#' err='{$defNokM112err}'/>

      <xsl:if test = "$mod_conf='monodinamic'">
        <cmd send='bridging\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>
      <xsl:if test = "$mod_conf!='monodinamic'">
        <cmd send='no bridging\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <cmd send='no ip rip-send\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='no ip rip-receive\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='ip address {$int_ip_router} {$int_mask_router}\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='vcc1\r' exp_ok='#' err='{$defNokM112err}'/>

      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='pvc {$vpi} {$vci} pppoe-llc\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='ip address 0.0.0.0 0.0.0.0\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='ppp username {$PPPuser}\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='ppp password {$PPPpasswd}\r' exp_ok='#' err='{$defNokM112err}'/>
        <cmd send='ppp authentication pap\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <xsl:if test = "$mod_conf!='multidinamic'">
        <xsl:if test = "$mod_conf='monodinamic'">
          <cmd send='pvc {$vpi} {$vci} eth-llc\r' exp_ok='#' err='{$defNokM112err}'/>
          <cmd send='bridging\r' exp_ok='#' err='{$defNokM112err}'/>
        </xsl:if>
        <xsl:if test = "$mod_conf!='monodinamic'">
          <cmd send='pvc {$vpi} {$vci} ip-llc\r' exp_ok='#' err='{$defNokM112err}'/>
      <cmd send='ip address {$ext_ip_router} {$ext_mask_router}\r' exp_ok='#' err='{$defNokM112err}'/>
        </xsl:if>
      </xsl:if>

      <xsl:if test = "$mod_conf='monodinamic'">
        <cmd send='no ip napt\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='monostatic'">
        <cmd send='no ip napt\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multistatic'">
        <cmd send='ip napt\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='ip napt\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <cmd send='no ip admin-disabled\r' exp_ok='#' err='{$defNokM112err}'/>

      <!-- FIXME: Port maps -->

      <cmd send='common\r' exp_ok='#' err='{$defNokM112err}'/>

      <cmd send='no ip route 0.0.0.0 vcc1\r' exp_ok='#' err='{$defNokM112err}'/>      

      <xsl:if test = "$mod_conf='monostatic'">
        <cmd send='ip route 0.0.0.0 0.0.0.0 {$ext_ip_router} vcc1\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <xsl:if test = "$mod_conf='multistatic'">
        <cmd send='ip route 0.0.0.0 0.0.0.0 {$ext_ip_router} vcc1\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='ip route 0.0.0.0 0.0.0.0 0.0.0.0 vcc1\r' exp_ok='#' err='{$defNokM112err}'/>
      </xsl:if>

      <cmd send='ip host-acl 193.152.37.192 255.255.255.240\r' exp_ok='#' err='{$defNokM112err}'/>
      <xsl:if test = "$mod_conf='monostatic'">
        <cmd send='ip host-acl {$dhcp_ip_net} {$dhcp_mask}\r' exp_ok='#'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multidinamic'">
        <cmd send='ip host-acl {$net_computer} {$mask_computer}\r' exp_ok='#'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='multistatic'">
        <cmd send='ip host-acl {$dhcp_ip_net} {$dhcp_mask}\r' exp_ok='#'/>
      </xsl:if>
      <xsl:if test = "$mod_conf='monodinamic'">
        <cmd send='ip host-acl 192.168.1.0 255.255.255.0\r' exp_ok='#'/>
      </xsl:if>

      <cmd send='quit\r' exp_ok='>' err='{$defNokM112err}'/>
      <cmd send='save config startup\r' exp_ok='>' err='{$defNokM112err}'/>
      <cmd send='logout\r' exp_ok='goodbye' err='{$defNokM112err}'/>
    </cmd_func>

    <cmd_func id='authNokiaM1112test'>
      <cmd send='\x17\x17\r' exp_ok='ddd>'> <!-- we are already logged -->
        <expect_list>
          <expect out='(login-id:|password:)'>
            <cmd send='1\r' exp_ok='>'>
              <expect_list>
                <expect out='(login-id:|password:)'>
                  <cmd send='11\r' exp_ok='>'>
                    <expect_list>
                      <expect out='(login-id:|password:)'> 
                        <!-- Wrong passwd (after three fails)-->
                        <cmd return='4'/> 
                      </expect>
                    </expect_list>
                  </cmd>
                </expect>
              </expect_list>
            </cmd>
          </expect>
          <expect out='\)#'>
            <cmd send='quit\r' exp_ok='>'/>
          </expect>
        </expect_list>
      </cmd>
    </cmd_func>
   
  </xsl:if>

</cmd_list>
</xsl:template>
</xsl:stylesheet>
