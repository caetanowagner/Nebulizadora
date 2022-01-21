window.onload = () =>
{
    getParams();
} ;

async function getParams()
{
  fetch("/api/get_params",{ method: "POST"}) 
  .then(function(response)
  {
    return response.json();
  })
  .then(function(json)
  {
  	/*	How to insert float inside text imput?	*/
    console.log(json);    
    document.getElementById("TimeDelay").value = parseInt(json.TimeDelay);
    document.getElementById("MotorStartLimit").value = parseInt(json.MaxStartsPerHour);
    document.getElementById("StartsPerHours").value = parseInt(json.NumStartsPerHour);
    document.getElementById("TotalOperHours").value = parseInt(json.TotalOperHours);
    document.getElementById("PartialOperHours").value = parseInt(json.PartialOperHours);
  }) 
  .catch(err => { 
    console.log('Fetch problem: ' + err.message);
  }) 
}
//setInterval(getParams, 60000);

function btnRefresh()
{
	getParams();
}

function btnApplyFunction()
{
  let pwd = document.getElementById("Password").value;
  let delay =  document.getElementById("TimeDelay").value;
  let startLimit =  document.getElementById("MotorStartLimit").value;  //alert(pwd.length);
  //alert(delay);
  
  if (pwd.length <=  1)
  {
    Password.focus();
    Password.value=''
  }
  else if (delay>1000 || delay < 0 || delay == '')
  {
    TimeDelay.focus();
    TimeDelay.value=''
  }
  else if (startLimit>60 || startLimit < 2 || startLimit == '')
  {  
    MotorStartLimit.focus();
    MotorStartLimit.value=''
  }
  else
  {
    delay = parseInt(delay);
    startLimit = parseInt(startLimit);

    let params = { TimeDelay: delay, MotorStartLimit: startLimit, Password: pwd };
    
    fetch("/api/set_params", { method: "POST", body: JSON.stringify(params) })
    .then(function(response)
    {
      return response.json();
    })
    .then(function(json)
    {
      let status = parseInt(json.flash);
      let message = "";
      console.log(json);      
      switch (status)
      {
        case "4":
          message = "Tempo de retardo fora dos limites permitidos.\nParâmetros não foram alterados!";
          break;
        case "3":
          message = "Quantidade de partidas do motor fora dos limites.\nParâmetros não foram alterados!";
          break;
        case "2":
          message = "Senha incorreta!\nParâmetros não foram alterados!";
          break;
        case "1":
          message = "Parâmetros alterados com sucesso!";
          break;
        default:
          message = "Erro desconhecido!\nParâmetros não foram alterados!";
          break;
      }
      //if (status == 1) getParams();
      alert (message);
    })
    .catch(function(err)
    {
      console.log('Fetch problem: ' + err.message);
    });
  }    
}

function btnResetFunction()
{ 
  let pwd = document.getElementById("Password").value;

  if (pwd.length <= 1)
  {
    Password.focus();
    Password.value=''
  }
  else
  {
    let params = { Password: pwd };
    fetch("/api/reset_params", { method: "POST", body: JSON.stringify(params) })
    .then(function(response)
    {
      return response.json();
    })
    .then(function(json)
    {
      let message = "";
      status = parseInt(json.flash);
      console.log(json);
      switch (status)
      {
        case "2":
          message = "Senha incorreta!\nParâmetros não foram alterados!";
          break;
        case "1":
          message = "Parâmetros alterados com sucesso!";
          break;
        default:
          message = "Erro desconhecido!\nParâmetros não foram alterados!";
          break;
      }
      if (status == 1) getParams();
      alert(message);

    })
    .catch(function(err)
    {
      console.log('Fetch problem: ' + err.message);
    });
  }
}