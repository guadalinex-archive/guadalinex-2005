<?xml version='1.0' encoding='iso-8859-1'?>
<xsl:stylesheet version="1.0"
 xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" indent="yes" encoding="iso-8859-1"/>
<xsl:template match="/">

<cmd_list>

  <!-- Vars -->

  <xsl:variable name="initial_cmd" select="//initial_cmd"/>
  <xsl:variable name="default_timeout" select="//default_timeout"/>

  <xsl:variable name="tty" select="//serial_params/@tty"/>
  <xsl:variable name="baudrate" select="//serial_params/@baudrate"/>
  <xsl:variable name="bits" select="//serial_params/@bits"/>
  <xsl:variable name="parity" select="//serial_params/@parity"/>
  <xsl:variable name="stopbits" select="//serial_params/@stopbits"/>
  <xsl:variable name="xonxoff" select="//serial_params/@xonxoff"/>  
  <xsl:variable name="rtscts" select="//serial_params/@rtscts"/>

  <xsl:variable name="passwd1" select="//passwd1"/>
  <xsl:variable name="passwd2" select="//passwd2"/>

  <!-- Common Commands -->

  <initial_cmd><xsl:value-of select="$initial_cmd"/></initial_cmd>
  <default_timeout><xsl:value-of select="$default_timeout"/></default_timeout>

  <serial_params 
      tty='{$tty}' 
      baudrate='{$baudrate}' 
      bits='{$bits}' 
      parity='{$parity}'
      stopbits='{$stopbits}'
      xonxoff='{$xonxoff}'
      rtscts='{$rtscts}'/>

  <cmd id='0004' type='exit' return='ok'/>
  <cmd id='0005' type='exit' return='error'/>
  <cmd id='5000' type='exit' return='eof'/>
  <cmd id='5001' type='exit' return='timeout'/>  

  <!-- 3Com OfficeConnect 812 -->

  <!-- 3Com OfficeConnect 812 - Test Access -->

  <cmd id='0001' type='sendline' cmd=''>
    <expect_list>
      <expect out='Password:' nextcmdid='0002'/>
      <expect out='(CLI -|>)' nextcmdid='0003'/>
    </expect_list>
  </cmd>
  <cmd id='0002' type='sendline' cmd='{$passwd1}'>
    <expect_list>
      <expect out='Password:' nextcmdid='0005'/>
      <expect out='Login incorrect' nextcmdid='0005'/>
      <expect out='>' nextcmdid='0003'/>
    </expect_list>
  </cmd>
  <cmd id='0003' type='sendline' cmd='exit cli'>
    <expect_list>
      <expect out='Password:' nextcmdid='0004'/>
      <expect out='(CLI - |Login incorrect)' nextcmdid='0005'/>
    </expect_list>
  </cmd>

  <!-- 3Com OfficeConnect 812 - Passwd chg -->

  <cmd id='0008' type='sendline' cmd=''>
    <expect_list>
      <expect out='Password:' nextcmdid='0009'/>
      <expect out='(CLI -|>)' nextcmdid='0010'/>
    </expect_list>
  </cmd>
  <cmd id='0009' type='sendline' cmd='{$passwd1}'>
    <expect_list>
      <expect out='Password:' nextcmdid='0005'/>
      <expect out='Login incorrect' nextcmdid='0005'/>
      <expect out='>' nextcmdid='0010'/>
    </expect_list>
  </cmd>
  <cmd id='0010' type='sendline' cmd='delete user {$passwd1}'>
    <expect_list>
      <expect out='>' nextcmdid='0011'/>
      <expect out='CLI - ' nextcmdid='0005'/>
    </expect_list>
  </cmd>
  <cmd id='0011' type='sendline' cmd='add user adminttd password adminttd'>
    <expect_list>
      <expect out='>' nextcmdid='0012'/>
      <expect out='CLI - ' nextcmdid='0005'/>
    </expect_list>
  </cmd>
  <cmd id='0012' type='sendline' cmd='enable command password {$passwd2}'>
    <expect_list>
      <expect out='>' nextcmdid='0013'/>
      <expect out='CLI - ' nextcmdid='0005'/>
    </expect_list>
  </cmd>  
  <cmd id='0013' type='sendline' cmd='add user {$passwd2} password {$passwd2}'>
    <expect_list>
      <expect out='>' nextcmdid='0014'/>
      <expect out='CLI - ' nextcmdid='0005'/>
    </expect_list>
  </cmd>  
  <cmd id='0014' type='sendline' cmd='save all'>
    <expect_list>
      <expect out='>' nextcmdid='0015'/>
      <expect out='CLI - ' nextcmdid='0005'/>
    </expect_list>
  </cmd>  
  <cmd id='0015' type='sendline' cmd='exit cli'>
    <expect_list>
      <expect out='>' nextcmdid='0004'/>
      <expect out='CLI - ' nextcmdid='0005'/>
    </expect_list>
  </cmd>

  <!-- Eficient SpeedStream -->

  <!-- Eficient SpeedStream - Test Access -->

  <!--  <initial_cmd>0001</initial_cmd>
       <serial_params tty='0' baudrate='38400' bits='8' parity='N' stopbits='1' xonxoff='1' rtscts='0'/>
       <default_timeout>25</default_timeout> -->

  <cmd id='0006' type='sendline' cmd='' timeout='5'>
    <expect_list>
      <expect out='Password:' nextcmdid='0007'/>
      <expect out='(Command->|for help)' nextcmdid='0004'/>
    </expect_list>
  </cmd>
  <cmd id='0007' type='sendline' cmd='{$passwd1}'>
    <expect_list>
      <expect out='Password:' nextcmdid='0005'/>
      <expect out='(Command->|for help)' nextcmdid='0004'/>
    </expect_list>
  </cmd>
  
</cmd_list>
</xsl:template>
</xsl:stylesheet>
