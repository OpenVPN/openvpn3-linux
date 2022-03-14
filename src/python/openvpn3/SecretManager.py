from gi.repository import Secret
import argparse
import getpass

#Each stored password has a set of attributes which are later used to lookup the password.
#The names and types of the attributes are defined in a schema. The schema is usually defined once globally.

OpenVPN3_LINUX = Secret.Schema.new('OpenVPN.Store',
    Secret.SchemaFlags.NONE,
    {
        'OpenVPN_Credential_Type': Secret.SchemaAttributeType.STRING,
        'Config': Secret.SchemaAttributeType.STRING,
    }
)


def store_credentials(Config):
    Username = input('Username: ')
    Password = getpass.getpass('Password: ')
    Secret.password_store_sync(OpenVPN3_LINUX, {'OpenVPN_Credential_Type': 'Password', 'Config': Config }, Secret.COLLECTION_DEFAULT,
                               'OpenVPN Password', Password, None)
    Secret.password_store_sync(OpenVPN3_LINUX, {'OpenVPN_Credential_Type': 'Username', 'Config': Config }, Secret.COLLECTION_DEFAULT,
                               'OpenVPN Username', Username, None)
    print("Credentials stored for " + config)

def update_credentials(Config):
    Username = input('Username: ')
    Password = getpass.getpass('Password: ')
    
    #Updates credentials by removing old ones and replacing with updated ones.
    #Returns True when success
    removed_password = Secret.password_clear_sync(OpenVPN3_LINUX, {'OpenVPN_Credential_Type': 'Password', 'Config': Config  }, None)
    if removed_password == False:
        print('Failed to remove existing Password, Do they exist?')
        exit()
    removed_username = Secret.password_clear_sync(OpenVPN3_LINUX, { 'OpenVPN_Credential_Type': 'Username', 'Config': Config }, None)
    if removed_username == False:
        print('Failed to remove existing credentials, Do they exist?')
        exit()
    Secret.password_store_sync(OpenVPN3_LINUX, {'OpenVPN_Credential_Type': 'Password', 'Config': Config }, Secret.COLLECTION_DEFAULT,
                               'OpenVPN Password', Password, None)
    Secret.password_store_sync(OpenVPN3_LINUX, {'OpenVPN_Credential_Type': 'String', 'Config': Config }, Secret.COLLECTION_DEFAULT,
                               'OpenVPN Username', Username, None)
    print("Credentials updated for " + config)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    subp = parser.add_subparsers(dest='subparser')
    store_credentials_parser = subp.add_parser('store', help='Stores new OpenVPN credentials.')
    store_credentials_parser.add_argument('--config', type=str)
    update_credentials_parser = subp.add_parser('update', help='Updates existing OpenVPN credentials.')
    update_credentials_parser.add_argument('--config', type=str)
    args = parser.parse_args()
    if args.subparser == 'store':
        store_credentials(args.config)
    elif args.subparser == 'update':
        update_credentials(args.config)






    
    
    
    
