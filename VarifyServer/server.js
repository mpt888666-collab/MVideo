const grpc = require('@grpc/grpc-js')
const crypto = require('crypto')
const message_proto = require('./proto')
const redis_module = require('./redis')
const emailModule = require('./email')
const const_module = require('./const')
const config_module = require('./config')

function GenerateVarifyCode() {
    return crypto.randomInt(0, 1000000).toString().padStart(6, '0')
}

async function GetVarifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try {
        let query_res = await redis_module.GetRedis(const_module.code_prefix + call.request.email);
        console.log("query_res is ", query_res)

        let uniqueId = query_res;
        if (uniqueId == null || !/^\d{6}$/.test(uniqueId)) {
            uniqueId = GenerateVarifyCode();
            let bres = await redis_module.SetRedisExpire(const_module.code_prefix + call.request.email, uniqueId, 600)
            if (!bres) {
                callback(null, {
                    email: call.request.email,
                    code: '',
                    error: const_module.Errors.RedisErr
                });
                return;
            }
        }

        console.log("uniqueId is ", uniqueId)
        let text_str = '您的验证码为' + uniqueId + '请三分钟内完成注册'

        let mailOptions = {
            from: config_module.email_user,
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };
        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        callback(null, {
            email: call.request.email,
            code: uniqueId,
            error: const_module.Errors.Success
        });
    } catch (error) {
        console.log("catch error is ", error)
        callback(null, {
            email: call.request.email,
            code: '',
            error: const_module.Errors.Exception
        });
    }
}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')
    })
}

main()